#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "dsp/DelayLine.h"
#include "dsp/PsolaShifter.h"
#include "dsp/YinDetector.h"

TEST_CASE("PSOLA shifts a 220 Hz sine up one semitone")
{
    constexpr double sr = 44100.0;
    const float ratio = std::exp2(1.f / 12.f);
    PsolaShifter p;
    p.prepare(sr);
    p.setParams(static_cast<float>(sr / 220.0), ratio);

    std::vector<float> out(30000);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = p.process(static_cast<float>(std::sin(2.0 * 3.14159265358979 * 220.0 * static_cast<double>(i) / sr)));

    YinDetector yin;
    yin.prepare(sr, 2048, 80.f);
    const auto r = yin.detect(out.data() + out.size() - 2048);
    REQUIRE(r.f0 > 0.f);
    REQUIRE(std::abs(1200.f * std::log2(r.f0 / (220.f * ratio))) < 15.f);
}

TEST_CASE("Dry delay line equals reported shifter latency")
{
    PsolaShifter p;
    p.prepare(48000.0);
    DelayLine d;
    d.prepare(p.getLatency());
    int peakAt = -1;
    for (int i = 0; i < p.getLatency() + 10; ++i)
        if (d.process(i == 0 ? 1.f : 0.f) > 0.5f) peakAt = i;
    REQUIRE(peakAt == p.getLatency());
}
