#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "ScaleQuantizer.h"

/// Pitch-correction logic, independent of JUCE. Call update() once per voiced
/// analysis hop with the detected pitch (fractional MIDI note); it returns the
/// shift (in semitones) the pitch shifter should apply.
///
/// Features
///  - Retune Speed : time constant of the correction glide (5 ms .. 405 ms).
///  - Humanize     : the longer a note is held, the slower the correction gets,
///                  so sustained notes keep their natural movement while short
///                  notes are still snapped quickly.
///  - Flex-Tune    : tolerance window around the target; small deviations are
///                  mostly left alone, large ones are fully corrected.
///  - Natural Vib. : vibrato is separated from the sung pitch (slow "centre"
///                  estimate). The centre is what gets tuned; the vibrato is
///                  then re-applied scaled by 0..200 % (0 = flattened).
///  - Artificial   : LFO vibrato (shape, rate, depth, variation, onset delay and
///    vibrato        onset rise) added on top of held notes.
///
/// Output pitch = target + naturalVibrato * (input - centre) + artificial vibrato.
/// No allocation; safe on the audio thread.
class CorrectionEngine {
public:
    struct Params {
        float amount = 1.f;         // 0..1   correction amount
        float speed = 0.5f;         // 0..1   1 = instant
        float humanize = 0.f;       // 0..1
        float flex = 0.f;           // 0..1   window half-width = flex * 0.3 st
        float naturalVibrato = 1.f; // 0..2   1 = keep, 0 = flatten, 2 = double
        int vibShape = 0;           // 0 sine, 1 triangle, 2 square, 3 saw
        float vibRateHz = 5.5f;
        float vibDepthCents = 0.f;  // peak deviation; 0 = off
        float vibVariation = 0.25f; // 0..1
        float vibDelaySec = 0.3f;   // time after note start before vibrato begins
        float vibRiseSec = 0.3f;    // time to reach full depth
    };

    struct Output {
        float shiftSemis = 0.f;
        int targetNote = -1;
        float centerMidi = 0.f;
        bool vibratoDetected = false;
    };

    void prepare(double hopSeconds) noexcept
    {
        dt = static_cast<float>(hopSeconds);
        winHops = std::clamp(static_cast<int>(std::lround(0.21 / hopSeconds)), 6, kHist - 2);
        gateCoef = 1.f - std::exp(-dt / 0.05f);
        gapLimit = std::max(4, static_cast<int>(std::lround(0.15 / hopSeconds)));
        reset();
    }

    void reset() noexcept
    {
        count = head = 0;
        vibActive = false;
        miss = gap = 0;
        gate = 0.f;
        curTarget = -1;
        hold = 0.f;
        corr = 0.f;
        vibPhase = 0.f;
    }

    /// Voiced hop. midi = detected pitch as fractional MIDI note.
    Output update(float midi, const ScaleQuantizer& q, const Params& p) noexcept
    {
        gap = 0;
        // Guard against wild detector values (octave jumps / glitches): never
        // let an out-of-range reading poison the history buffer.
        if (!std::isfinite(midi)) return lastOutput();
        midi = std::clamp(midi, 24.f, 108.f);
        push(midi);

        // ---- 1. Vibrato analysis: slow centre + "is this vibrato?" test -----
        float slow = midi;
        bool criteria = false;
        float spread = 0.f;
        if (count >= winHops) {
            slow = mean(winHops);
            float mn = 1e9f, mx = -1e9f;
            int cross = 0, prev = 0;
            for (int i = winHops - 1; i >= 0; --i) {
                const float v = at(i);
                const float d = v - slow;
                mn = std::min(mn, v);
                mx = std::max(mx, v);
                const int sg = d > 0.02f ? 1 : (d < -0.02f ? -1 : 0);
                if (sg != 0) {
                    if (prev != 0 && sg != prev) ++cross;
                    prev = sg;
                }
            }
            spread = mx - mn;
            criteria = spread >= 0.15f && spread <= 2.8f && cross >= 2;
        }

        // A jump much larger than the vibrato extent is a new note, not vibrato.
        if ((vibActive || gate > 0.001f) && std::abs(midi - slow) > 1.3f * (0.5f * spread) + 0.25f) {
            vibActive = false;
            gate = 0.f;
            miss = 0;
            count = 1; // keep only the newest value
            slow = midi;
            criteria = false;
        }

        if (criteria) {
            vibActive = true;
            miss = 0;
        } else if (vibActive && ++miss > 5) {
            vibActive = false;
        }
        gate += ((vibActive ? 1.f : 0.f) - gate) * gateCoef;
        const float center = midi + gate * (slow - midi);

        // ---- 2. Target note with hysteresis ---------------------------------
        hold += dt;
        const int nearest = q.nearestNote(center);
        if (curTarget < 0 || !q.inScale(curTarget)
            || (nearest != curTarget
                && std::abs(center - static_cast<float>(curTarget)) - std::abs(center - static_cast<float>(nearest)) > 0.12f)) {
            curTarget = nearest;
            hold = 0.f;
            vibPhase = 0.f;
        }

        // ---- 3. Flex-Tune: soft-knee on the deviation -----------------------
        const float dev = static_cast<float>(curTarget) - center;
        const float halfWin = p.flex * 0.3f;
        float devEff = dev;
        if (halfWin > 1e-4f) {
            const float x = std::abs(dev);
            const float u = std::min(1.f, x / (2.f * halfWin));
            devEff = std::copysign(x * u * u * (3.f - 2.f * u), dev);
        }
        const float desired = p.amount * devEff;

        // ---- 4. Retune speed + Humanize -------------------------------------
        const float spd = std::clamp(p.speed, 0.f, 1.f);
        const float tau0 = 0.005f + 0.4f * (1.f - spd) * (1.f - spd);
        const float sustained = smooth01((hold - 0.08f) / 0.6f);
        const float tau = tau0 + std::clamp(p.humanize, 0.f, 1.f) * sustained * 0.5f;
        corr += (1.f - std::exp(-dt / tau)) * (desired - corr);

        // ---- 5. Assemble the shift ------------------------------------------
        float shift = corr + (p.naturalVibrato - 1.f) * (midi - center) + artificialVibrato(p);
        shift = std::clamp(shift, -12.f, 12.f);

        Output o;
        o.shiftSemis = shift;
        o.targetNote = curTarget;
        o.centerMidi = center;
        o.vibratoDetected = vibActive;
        last = o;
        return o;
    }

    /// Unvoiced hop. Returns true when the state was reset after a long gap.
    bool unvoiced() noexcept
    {
        if (++gap > gapLimit && curTarget >= 0) {
            reset();
            return true;
        }
        return false;
    }

private:
    static constexpr int kHist = 64; // power of two

    Output lastOutput() const noexcept { return last; }

    static float smooth01(float x) noexcept
    {
        x = std::clamp(x, 0.f, 1.f);
        return x * x * (3.f - 2.f * x);
    }

    void push(float v) noexcept
    {
        hist[static_cast<size_t>(head)] = v;
        head = (head + 1) & (kHist - 1);
        count = std::min(count + 1, kHist);
    }
    float at(int ago) const noexcept // 0 = newest
    {
        return hist[static_cast<size_t>((head - 1 - ago) & (kHist - 1))];
    }
    float mean(int n) const noexcept
    {
        float s = 0.f;
        for (int i = 0; i < n; ++i) s += at(i);
        return s / static_cast<float>(n);
    }

    float artificialVibrato(const Params& p) noexcept
    {
        constexpr float kTwoPi = 6.2831853f;
        // Two slow, incommensurate LFOs give "variation" without needing a RNG.
        var1 += 0.37f * dt; var1 -= std::floor(var1);
        var2 += 0.61f * dt; var2 -= std::floor(var2);
        const float n1 = 0.5f * (std::sin(kTwoPi * var1) + std::sin(kTwoPi * var2 * 1.7f));
        const float n2 = 0.5f * (std::sin(kTwoPi * var2) + std::sin(kTwoPi * var1 * 2.3f));
        const float v = std::clamp(p.vibVariation, 0.f, 1.f);

        const float rate = std::max(0.1f, p.vibRateHz * (1.f + 0.25f * v * n1));
        vibPhase += rate * dt;
        vibPhase -= std::floor(vibPhase);
        if (p.vibDepthCents <= 0.f) return 0.f;

        const float depthSemis = 0.01f * p.vibDepthCents * (1.f + 0.4f * v * n2);
        const float x = p.vibRiseSec > 1e-3f ? (hold - p.vibDelaySec) / p.vibRiseSec
                                             : (hold >= p.vibDelaySec ? 1.f : 0.f);
        return depthSemis * smooth01(x) * waveform(p.vibShape, vibPhase);
    }

    static float waveform(int shape, float ph) noexcept
    {
        switch (shape) {
            case 1: return ph < 0.25f ? 4.f * ph : (ph < 0.75f ? 2.f - 4.f * ph : 4.f * ph - 4.f);
            case 2: return std::tanh(5.f * std::sin(6.2831853f * ph));
            case 3: { float f = ph + 0.5f; f -= std::floor(f); return 2.f * f - 1.f; }
            default: return std::sin(6.2831853f * ph);
        }
    }

    float dt = 0.0116f, gateCoef = 0.2f;
    int winHops = 18, gapLimit = 13;

    std::array<float, kHist> hist{};
    int count = 0, head = 0;

    bool vibActive = false;
    int miss = 0, gap = 0;
    float gate = 0.f;

    int curTarget = -1;
    float hold = 0.f, corr = 0.f;
    float vibPhase = 0.f, var1 = 0.f, var2 = 0.17f;
    Output last{};
};
