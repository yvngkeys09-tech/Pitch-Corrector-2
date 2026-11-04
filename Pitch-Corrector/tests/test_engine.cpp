#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <functional>
#include <vector>
#include "dsp/CorrectionEngine.h"

namespace {
constexpr double kSr = 44100.0;
constexpr int kHop = 512;
constexpr double kDt = kHop / kSr;
constexpr double kPi = 3.14159265358979;

struct Stats { double lo, hi, mean; };

// Feeds a synthetic pitch track (MIDI note vs time) through the engine and
// returns the corrected pitch (input + shift) for every hop.
std::vector<double> run(const CorrectionEngine::Params& p, double seconds,
                        const std::function<double(double)>& midiAt,
                        int root = 0, ScaleQuantizer::Scale sc = ScaleQuantizer::Scale::Chromatic)
{
    CorrectionEngine e;
    e.prepare(kDt);
    ScaleQuantizer q;
    q.set(root, sc);
    std::vector<double> out;
    for (double t = 0; t < seconds; t += kDt) {
        const double m = midiAt(t);
        out.push_back(m + static_cast<double>(e.update(static_cast<float>(m), q, p).shiftSemis));
    }
    return out;
}

Stats window(const std::vector<double>& v, double t0, double t1)
{
    Stats s{1e9, -1e9, 0.0};
    int n = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        const double t = static_cast<double>(i) * kDt;
        if (t < t0 || t > t1) continue;
        s.lo = std::min(s.lo, v[i]);
        s.hi = std::max(s.hi, v[i]);
        s.mean += v[i];
        ++n;
    }
    s.mean /= std::max(1, n);
    return s;
}

CorrectionEngine::Params fast()
{
    CorrectionEngine::Params p;
    p.speed = 1.f;
    p.vibDepthCents = 0.f;
    return p;
}

// 30 cents sharp of C4, +/-40 cents of 5.5 Hz vibrato.
double vibratoTrack(double t) { return 60.3 + 0.4 * std::sin(2.0 * kPi * 5.5 * t); }
} // namespace

TEST_CASE("Natural Vibrato 0 % flattens vibrato onto the target note")
{
    auto p = fast();
    p.naturalVibrato = 0.f;
    const auto s = window(run(p, 2.0, vibratoTrack), 0.9, 2.0);
    REQUIRE(s.hi - s.lo < 0.1);
    REQUIRE(std::abs(s.mean - 60.0) < 0.03);
}

TEST_CASE("Natural Vibrato 100 % keeps vibrato but tunes its centre")
{
    auto p = fast();
    p.naturalVibrato = 1.f;
    const auto s = window(run(p, 2.0, vibratoTrack), 0.9, 2.0);
    REQUIRE(s.hi - s.lo > 0.65);
    REQUIRE(s.hi - s.lo < 0.95);
    REQUIRE(std::abs(s.mean - 60.0) < 0.08);
}

TEST_CASE("Natural Vibrato 200 % doubles vibrato depth")
{
    auto p = fast();
    p.naturalVibrato = 2.f;
    const auto s = window(run(p, 2.0, vibratoTrack), 0.9, 2.0);
    REQUIRE(s.hi - s.lo > 1.3);
    REQUIRE(s.hi - s.lo < 1.9);
    REQUIRE(std::abs(s.mean - 60.0) < 0.1);
}

TEST_CASE("Note changes are not mistaken for vibrato")
{
    auto p = fast();
    p.naturalVibrato = 0.f;
    const auto out = run(p, 2.5, [](double t) { return t < 1.2 ? 60.3 : 62.2; });
    const auto a = window(out, 0.5, 1.1);
    const auto b = window(out, 1.5, 2.5);
    REQUIRE(std::abs(a.mean - 60.0) < 0.05);
    REQUIRE(std::abs(b.mean - 62.0) < 0.05);
    REQUIRE(b.hi - b.lo < 0.05);
}

TEST_CASE("Humanize lets sustained notes keep their natural drift")
{
    // Note starts in tune, then drifts 30 cents sharp between 0.8 s and 1.0 s.
    auto track = [](double t) { return 60.0 + 0.3 * std::clamp((t - 0.8) / 0.2, 0.0, 1.0); };
    auto p = fast();
    p.humanize = 0.f;
    const double plain = window(run(p, 1.2, track), 1.05, 1.15).mean - 60.0;
    p.humanize = 1.f;
    const double human = window(run(p, 1.2, track), 1.05, 1.15).mean - 60.0;
    REQUIRE(std::abs(plain) < 0.05);  // snapped back to the note
    REQUIRE(human > plain + 0.12);    // drift largely preserved
}

TEST_CASE("Humanize does not slow down the start of a note")
{
    auto p = fast();
    p.humanize = 1.f;
    const auto out = run(p, 0.4, [](double) { return 60.3; });
    REQUIRE(std::abs(window(out, 0.25, 0.4).mean - 60.0) < 0.05);
}

TEST_CASE("Flex-Tune leaves small deviations alone")
{
    auto track = [](double) { return 60.2; };
    auto p = fast();
    p.flex = 0.f;
    REQUIRE(std::abs(window(run(p, 1.0, track), 0.5, 1.0).mean - 60.0) < 0.03);
    p.flex = 1.f;
    REQUIRE(window(run(p, 1.0, track), 0.5, 1.0).mean > 60.1);
}

TEST_CASE("Flex-Tune still corrects large deviations")
{
    auto p = fast();
    p.flex = 0.5f; // window half-width 0.15 st -> full correction beyond 0.3 st
    REQUIRE(std::abs(window(run(p, 1.0, [](double) { return 60.45; }), 0.5, 1.0).mean - 60.0) < 0.03);
}

TEST_CASE("Artificial vibrato adds the requested depth after the onset delay")
{
    auto p = fast();
    p.vibDepthCents = 50.f;
    p.vibRateHz = 5.f;
    p.vibVariation = 0.f;
    p.vibDelaySec = 0.5f;
    p.vibRiseSec = 0.2f;
    const auto out = run(p, 2.0, [](double) { return 60.0; });
    const auto early = window(out, 0.0, 0.4);
    const auto late = window(out, 1.0, 2.0);
    REQUIRE(early.hi - early.lo < 0.02);
    REQUIRE(late.hi - late.lo > 0.9);
    REQUIRE(late.hi - late.lo < 1.1);
    REQUIRE(std::abs(late.mean - 60.0) < 0.05);
}

TEST_CASE("Targets respect the selected scale")
{
    auto p = fast();
    const auto out = run(p, 1.0, [](double) { return 60.9; }, 0, ScaleQuantizer::Scale::Major);
    REQUIRE(std::abs(window(out, 0.5, 1.0).mean - 60.0) < 0.03); // C#4 -> C4 in C major
}
