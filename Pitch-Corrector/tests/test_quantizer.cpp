#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "dsp/ScaleQuantizer.h"

TEST_CASE("Chromatic rounds to nearest semitone")
{
    ScaleQuantizer q;
    q.set(0, ScaleQuantizer::Scale::Chromatic);
    REQUIRE(q.nearestNote(60.4f) == 60);
    REQUIRE(q.nearestNote(60.6f) == 61);
    REQUIRE(q.nearestNote(ScaleQuantizer::hzToMidi(440.f)) == 69);
}

TEST_CASE("C major skips out-of-scale notes")
{
    ScaleQuantizer q;
    q.set(0, ScaleQuantizer::Scale::Major);
    REQUIRE(q.nearestNote(60.9f) == 60); // between C4 and D4, C#4 is not in the scale
    REQUIRE(q.nearestNote(61.2f) == 62);
    REQUIRE(q.nearestNote(66.0f) != 66); // F#4 not in C major
    REQUIRE(q.inScale(62));
    REQUIRE(!q.inScale(61));
}

TEST_CASE("Ratio moves detected pitch onto target")
{
    ScaleQuantizer q;
    q.set(0, ScaleQuantizer::Scale::Chromatic);
    REQUIRE(q.targetRatio(450.f) * 450.f == Catch::Approx(440.f).margin(0.01));
}
