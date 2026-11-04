#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

/// Streaming TD-PSOLA pitch shifter with a fixed, exactly reported latency.
///
/// Time map: output sample n corresponds to input sample n - latency.
/// Synthesis marks are placed every period/ratio samples. For each one, the
/// nearest analysis mark (a lattice spaced one period apart, snapped to local
/// peaks for phase coherence) supplies a 2-period Hann grain. The grain is
/// overlap-added at the synthesis mark and scaled by 1/ratio for unity gain.
/// Grain shapes are not resampled, so formants stay put.
class PsolaShifter {
public:
    void prepare(double sampleRate, float minHz = 80.f, float maxHz = 1000.f)
    {
        maxPeriod = std::max(16, static_cast<int>(std::ceil(sampleRate / minHz)));
        minPeriod = std::max(8, static_cast<int>(sampleRate / maxHz));
        if (minPeriod > maxPeriod) minPeriod = maxPeriod;
        // Grains read up to ~1.75 periods past the analysis centre and are placed
        // up to one period ahead of the output cursor.
        latency = static_cast<int>(std::ceil(2.75 * maxPeriod)) + 4;
        std::size_t sz = 1;
        while (sz < static_cast<std::size_t>(latency + 4 * maxPeriod + 64)) sz <<= 1;
        mask = static_cast<int64_t>(sz) - 1;
        inBuf.assign(sz, 0.f);
        outBuf.assign(sz, 0.f);
        period = static_cast<float>(sampleRate / 200.0);
        ratio = 1.f;
        reset();
    }

    void reset() noexcept
    {
        std::fill(inBuf.begin(), inBuf.end(), 0.f);
        std::fill(outBuf.begin(), outBuf.end(), 0.f);
        n = 0;
        nextMark = 0.0;
        anchored = false;
        aCenter = 0;
    }

    int getLatency() const noexcept { return latency; }

    void setParams(float periodSamples, float pitchRatio) noexcept
    {
        period = std::clamp(periodSamples, static_cast<float>(minPeriod),
                            static_cast<float>(maxPeriod));
        ratio = std::clamp(pitchRatio, 0.5f, 2.0f);
    }

    float process(float x) noexcept
    {
        inBuf[static_cast<std::size_t>(n & mask)] = x;
        const int Tc = static_cast<int>(std::lround(period));
        while (nextMark <= static_cast<double>(n + Tc)) {
            placeGrain(static_cast<int64_t>(std::llround(nextMark)));
            nextMark += static_cast<double>(period) / static_cast<double>(ratio);
        }
        const auto idx = static_cast<std::size_t>(n & mask);
        const float y = outBuf[idx];
        outBuf[idx] = 0.f;
        ++n;
        return y;
    }

private:
    int64_t snap(int64_t c, int T) const noexcept
    {
        int64_t best = c;
        float bv = -1e30f;
        for (int k = -T / 4; k <= T / 4; ++k) {
            const float v = inBuf[static_cast<std::size_t>((c + k) & mask)];
            if (v > bv) { bv = v; best = c + k; }
        }
        return best;
    }

    void placeGrain(int64_t ts) noexcept
    {
        const int T = static_cast<int>(std::lround(period));
        const int64_t ta = ts - latency;
        if (!anchored || std::llabs(ta - aCenter) > 3 * T) {
            aCenter = snap(ta, T);
            anchored = true;
        }
        while (ta - aCenter > T / 2) aCenter = snap(aCenter + T, T);
        while (aCenter - ta > T / 2) aCenter = snap(aCenter - T, T);

        const float gain = 1.f / ratio;
        const int64_t first = std::max<int64_t>(-T, n - ts); // never write the past
        for (int64_t k = first; k <= T; ++k) {
            const float w = 0.5f * (1.f + std::cos(3.14159265f * static_cast<float>(k) / static_cast<float>(T)));
            outBuf[static_cast<std::size_t>((ts + k) & mask)] +=
                gain * w * inBuf[static_cast<std::size_t>((aCenter + k) & mask)];
        }
    }

    std::vector<float> inBuf, outBuf;
    int64_t mask = 0, n = 0, aCenter = 0;
    double nextMark = 0.0;
    float period = 220.f, ratio = 1.f;
    int minPeriod = 8, maxPeriod = 552, latency = 1520;
    bool anchored = false;
};
