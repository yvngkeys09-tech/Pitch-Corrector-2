#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

/// YIN monophonic pitch detector. All buffers are allocated in prepare(),
/// so detect() is safe to call from the audio thread.
class YinDetector {
public:
    struct Result { float f0 = 0.f; float confidence = 0.f; }; // f0 = 0 -> unvoiced

    void prepare(double sampleRate, int frameSize, float minHz = 70.f)
    {
        sr = sampleRate;
        N = frameSize;
        tauMax = std::min(N / 2, static_cast<int>(sr / minHz));
        diff.assign(static_cast<size_t>(tauMax + 1), 0.f);
        cmnd.assign(static_cast<size_t>(tauMax + 1), 1.f);
    }

    // frame must contain at least N samples.
    Result detect(const float* x, float threshold = 0.15f) noexcept
    {
        const int W = N - tauMax; // integration window
        diff[0] = 0.f;
        for (int tau = 1; tau <= tauMax; ++tau) {
            float s = 0.f;
            for (int i = 0; i < W; ++i) {
                const float d = x[i] - x[i + tau];
                s += d * d;
            }
            diff[static_cast<size_t>(tau)] = s;
        }

        // Cumulative mean normalised difference
        cmnd[0] = 1.f;
        float running = 0.f;
        for (int tau = 1; tau <= tauMax; ++tau) {
            running += diff[static_cast<size_t>(tau)];
            cmnd[static_cast<size_t>(tau)] =
                running > 0.f ? diff[static_cast<size_t>(tau)] * static_cast<float>(tau) / running : 1.f;
        }

        // Absolute threshold: first dip below it, then walk to the local minimum
        int tauEst = -1;
        for (int tau = 2; tau < tauMax; ++tau) {
            if (cmnd[static_cast<size_t>(tau)] < threshold) {
                while (tau + 1 < tauMax && cmnd[static_cast<size_t>(tau + 1)] < cmnd[static_cast<size_t>(tau)]) ++tau;
                tauEst = tau;
                break;
            }
        }
        if (tauEst < 0) return {}; // unvoiced / low confidence

        // Parabolic interpolation around the minimum
        const float a = cmnd[static_cast<size_t>(tauEst - 1)];
        const float b = cmnd[static_cast<size_t>(tauEst)];
        const float c = cmnd[static_cast<size_t>(tauEst + 1)];
        const float denom = a - 2.f * b + c;
        const float shift = std::abs(denom) > 1e-12f ? 0.5f * (a - c) / denom : 0.f;
        const float tauRefined = static_cast<float>(tauEst) + shift;

        Result r;
        r.f0 = static_cast<float>(sr) / tauRefined;
        r.confidence = 1.f - std::min(1.f, b);
        return r;
    }

private:
    double sr = 44100.0;
    int N = 2048, tauMax = 1024;
    std::vector<float> diff, cmnd;
};
