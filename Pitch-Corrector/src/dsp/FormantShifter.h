#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

/// Formant (vocal-timbre) shifter, independent of JUCE. Safe on the audio thread.
///
/// The signal is split by a 4th-order Linkwitz-Riley crossover at fc into a low
/// band (fundamental + F1, below fc) and a high band (upper formants and
/// sibilance, above fc). Only the high band is spectrally shifted: a fractional
/// delay line with cubic interpolation compresses or stretches its time axis,
/// multiplying its spectrum by 1/shift. Formants therefore move down for bigger
/// voices (Bass, Low Male) and up for smaller ones (Soprano, Instrument) while
/// pitch, vibrato timing and the low end stay untouched.
///
/// Filter algebra: let B be one Q = 1/sqrt(2) Butterworth low-pass section with
/// numerator A(1+z^-1)^2 and denominator 1 + D z^-1 + E z^-2. The LR4 low band
/// is L = B^2; the exact complementary high band is
///     H = (z^-2 - L) / (1 - A),   so   L + (1 - A) H = z^-2  (flat sum).
/// H's denominator is the reversed B^2 denominator, hence stable whenever L is.
class FormantShifter {
public:
    static constexpr int kMaxChannels = 2;

    void prepare(double sampleRate, int numChannels)
    {
        sr = static_cast<float>(sampleRate);
        fc = std::clamp(650.f * (sr / 44100.f), 300.f, 1200.f);
        const double w = static_cast<double>(fc) * 2.0 * kPi / static_cast<double>(sr);
        const double cw = std::cos(w), sw = std::sin(w);
        const double alpha = sw * 0.7071067811865476;   // sin(w)/(2Q), Q = 1/sqrt(2)
        const double k = 1.0 + alpha;
        const double A = (1.0 - cw) * 0.5 / k;          // B numerator scale
        const double D = -2.0 * cw / k;                 // B a1
        const double E = (1.0 - alpha) / k;             // B a2
        // One B section, applied twice, forms L = B^2.
        bCoef = {{static_cast<float>(A), static_cast<float>(2.0 * A), static_cast<float>(A),
                  static_cast<float>(D), static_cast<float>(E)}};
        // denL = (1 + D z^-1 + E z^-2)^2 ; numL = A (1 + 2 z^-1 + z^-2).
        const float denL[4] = {static_cast<float>(2.0 * D), static_cast<float>(D * D + 2.0 * E),
                               static_cast<float>(2.0 * D * E), static_cast<float>(E * E)};
        const float numL[3] = {static_cast<float>(A), static_cast<float>(2.0 * A), static_cast<float>(A)};
        // H = (z^-2 - L)/(1-A): numerator g*(rev(denL) padded - numL padded),
        // denominator rev(denL). Both are quartics in z^-1.
        const double g = 1.0 / (1.0 - A);
        for (int i = 0; i < 5; ++i) {
            const double rd = (i < 4 ? static_cast<double>(denL[3 - i]) : 1.0); // rev(denL)+z^-4
            const double nl = (i < 3 ? static_cast<double>(numL[i]) : 0.0);
            hNum[i] = static_cast<float>(g * (rd - nl));
        }
        for (int i = 0; i < 4; ++i) hDen[i] = denL[3 - i];

        maxDelay = std::max(8, static_cast<int>(std::lround(0.0075f * sr))); // 7.5 ms span
        std::size_t sz = 1;
        while (sz < static_cast<std::size_t>(maxDelay) + 8) sz <<= 1;
        mask = sz - 1;
        for (auto& c : ch) {
            c.buf.assign(sz, 0.f);
            resetChannel(c);
        }
        this->numCh = std::clamp(numChannels, 1, kMaxChannels);
        setRatio(1.f);
    }

    void reset() noexcept
    {
        for (auto& c : ch) {
            std::fill(c.buf.begin(), c.buf.end(), 0.f);
            resetChannel(c);
        }
    }

    /// 1 = unchanged timbre; 0.7 ~ -5.7 st darker, 1.4 ~ +5.7 st brighter.
    void setRatio(float r) noexcept { ratio = std::clamp(r, 0.65f, 1.55f); }

    float process(int channel, float x) noexcept
    {
        auto& c = ch[static_cast<std::size_t>(channel)];
        // L = B^2(x): the same Q = 0.7071 section applied twice.
        const float l1 = biquad(x, c.lb1, bCoef[0], bCoef[1], bCoef[2], bCoef[3], bCoef[4]);
        const float lp = biquad(l1, c.lb2, bCoef[0], bCoef[1], bCoef[2], bCoef[3], bCoef[4]);
        // H: direct form II transposed fourth-order section.
        float hp = hNum[0] * x + c.hz[0];
        c.hz[0] = hNum[1] * x - hDen[0] * hp + c.hz[1];
        c.hz[1] = hNum[2] * x - hDen[1] * hp + c.hz[2];
        c.hz[2] = hNum[3] * x - hDen[2] * hp + c.hz[3];
        c.hz[3] = hNum[4] * x - hDen[3] * hp;

        // Fractional read ahead of the write cursor compresses the high band's
        // time axis by 'ratio', scaling its spectrum up by 1/ratio.
        c.buf[c.pos] = hp;
        const float d = static_cast<float>(maxDelay) - (static_cast<float>(maxDelay) - 1.f) * ratio;
        return lp + readFrac(c, d);
    }

private:
    static constexpr double kPi = 3.14159265358979323846;

    struct StageState {
        float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;
    };

    struct Channel {
        std::vector<float> buf;
        std::size_t pos = 0;
        StageState lb1, lb2;
        float hz[4] = {0.f, 0.f, 0.f, 0.f};
    };

    void resetChannel(Channel& c) noexcept
    {
        c.pos = 0;
        c.lb1 = {}; c.lb2 = {};
        c.hz[0] = c.hz[1] = c.hz[2] = c.hz[3] = 0.f;
    }

    static float biquad(float x, StageState& s, float b0, float b1, float b2,
                        float a1, float a2) noexcept
    {
        const float y = b0 * x + b1 * s.x1 + b2 * s.x2 - a1 *s.y1 - a2 * s.y2;
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return y;
    }

    float readFrac(const Channel& c, float d) const noexcept
    {
        const float fd = std::clamp(d, 0.f, static_cast<float>(maxDelay));
        const auto i0 = static_cast<int>(fd);
        const float f = fd - static_cast<float>(i0);
        const auto m = static_cast<std::size_t>(c.pos) + static_cast<std::size_t>(maxDelay);
        const unsigned ii = static_cast<unsigned>(i0);
        const float xm2 = c.buf[(m - 2u - ii) & mask];
        const float xm1 = c.buf[(m - 1u - ii) & mask];
        const float x0 = c.buf[(m - ii) & mask];
        const float xp1 = c.buf[(m + 1u - ii) & mask];
        // Catmull-Rom cubic interpolation.
        const float a = -0.5f * xm2 + 1.5f * xm1 - 1.5f * x0 + 0.5f * xp1;
        const float b = xm2 - 2.5f * xm1 + 2.f * x0 - 0.5f * xp1;
        const float cc = -0.5f * xm1 + 0.5f * x0;
        return ((a * f + b) * f + cc) * f + x0;
    }

    std::array<Channel, kMaxChannels> ch;
    std::size_t mask = 1;
    float sr = 44100.f, fc = 650.f, ratio = 1.f;
    std::array<float, 5> bCoef{};   // B section (used twice for L)
    float hNum[5] = {}, hDen[4] = {};
    int maxDelay = 330, numCh = 2;
};
