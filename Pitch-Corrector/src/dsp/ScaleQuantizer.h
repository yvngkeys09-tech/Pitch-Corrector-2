#pragma once
#include <array>
#include <cmath>

/// Maps pitches onto the nearest note of a selected scale.
class ScaleQuantizer {
public:
    enum class Scale { Chromatic, Major, Minor, HarmonicMinor, Pentatonic };

    void set(int rootPitchClass, Scale s) noexcept
    {
        root = ((rootPitchClass % 12) + 12) % 12;
        mask = maskFor(s);
    }

    static float hzToMidi(float hz) noexcept { return 69.f + 12.f * std::log2(hz / 440.f); }
    static float midiToHz(float m) noexcept { return 440.f * std::exp2((m - 69.f) / 12.f); }

    // Nearest in-scale MIDI note to a fractional MIDI value.
    int nearestNote(float midi) const noexcept
    {
        const int base = static_cast<int>(std::floor(midi));
        int best = base;
        float bestDist = 1e9f;
        for (int n = base - 2; n <= base + 3; ++n) {
            if (!mask[static_cast<size_t>(((n - root) % 12 + 12) % 12)]) continue;
            const float d = std::abs(midi - static_cast<float>(n));
            if (d < bestDist) { bestDist = d; best = n; }
        }
        return best;
    }

    bool inScale(int note) const noexcept
    {
        return mask[static_cast<size_t>(((note - root) % 12 + 12) % 12)];
    }

    float targetRatio(float detectedHz) const noexcept
    {
        const int target = nearestNote(hzToMidi(detectedHz));
        return midiToHz(static_cast<float>(target)) / detectedHz;
    }

private:
    using Mask = std::array<bool, 12>;
    static Mask maskFor(Scale s) noexcept
    {
        switch (s) {
            case Scale::Major:         return {true,false,true,false,true,true,false,true,false,true,false,true};
            case Scale::Minor:         return {true,false,true,true,false,true,false,true,true,false,true,false};
            case Scale::HarmonicMinor: return {true,false,true,true,false,true,false,true,true,false,false,true};
            case Scale::Pentatonic:    return {true,false,true,false,true,false,false,true,false,true,false,false};
            default:                   return {true,true,true,true,true,true,true,true,true,true,true,true};
        }
    }
    int root = 0;
    Mask mask = {true,true,true,true,true,true,true,true,true,true,true,true};
};
