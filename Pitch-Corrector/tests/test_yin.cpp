#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "dsp/YinDetector.h"

TEST_CASE("YIN detects sines within 5 cents")
{
    constexpr double sr = 44100.0;
    for (float f : {110.f, 220.f, 440.f}) {
        YinDetector yin;
        yin.prepare(sr, 2048, 80.f);
        std::vector<float> x(2048);
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = static_cast<float>(std::sin(2.0 * 3.14159265358979 * f * static_cast<double>(i) / sr));
        const auto r = yin.detect(x.data());
        REQUIRE(r.f0 > 0.f);
        REQUIRE(std::abs(1200.f * std::log2(r.f0 / f)) < 5.f);
    }
}

TEST_CASE("YIN rejects silence")
{
    YinDetector yin;
    yin.prepare(44100.0, 2048, 80.f);
    std::vector<float> x(2048, 0.f);
    REQUIRE(yin.detect(x.data()).f0 == 0.f);
}
