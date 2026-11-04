#pragma once
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/ScaleQuantizer.h"
#include "PluginLookAndFeel.h"

/// Piano strip (C2..C7). Notes in the chosen scale are tinted, the target note
/// is lit magenta, the detected note carries a white dot. Clicking a key sets
/// the scale's root note.
class KeyboardDisplay : public juce::Component {
public:
    KeyboardDisplay(juce::AudioProcessorValueTreeState& s, const std::atomic<float>& det,
                    const std::atomic<float>& tgt)
        : apvts(s), detected(det), target(tgt)
    {
        rootValue = apvts.getRawParameterValue("root");
        scaleValue = apvts.getRawParameterValue("scale");
    }

    void tick()
    {
        const int sc = static_cast<int>(scaleValue->load());
        tint = sc != 0; // chromatic would tint every key
        quantizer.set(static_cast<int>(rootValue->load()), static_cast<ScaleQuantizer::Scale>(sc));
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const float t = target.load(std::memory_order_relaxed);
        const float d = detected.load(std::memory_order_relaxed);
        const int tgt = t >= 0.f ? juce::roundToInt(t) : -1;
        const int det = d >= 0.f ? juce::roundToInt(d) : -1;
        for (int n = kLow; n <= kHigh; ++n) if (!isBlack(n)) drawKey(g, n, tgt, det);
        for (int n = kLow; n <= kHigh; ++n) if (isBlack(n)) drawKey(g, n, tgt, det);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        int hit = -1;
        for (int n = kLow; n <= kHigh && hit < 0; ++n)
            if (isBlack(n) && keyRect(n).contains(p)) hit = n;
        for (int n = kLow; n <= kHigh && hit < 0; ++n)
            if (!isBlack(n) && keyRect(n).contains(p)) hit = n;
        if (hit < 0) return;
        if (auto* prm = apvts.getParameter("root")) {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost(prm->convertTo0to1(static_cast<float>(hit % 12)));
            prm->endChangeGesture();
        }
    }

private:
    static constexpr int kLow = 36, kHigh = 96, kWhites = 36;

    static bool isBlack(int n) noexcept
    {
        const int pc = n % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }
    static int whiteIndex(int n) noexcept
    {
        int c = 0;
        for (int i = kLow; i < n; ++i) if (!isBlack(i)) ++c;
        return c;
    }
    juce::Rectangle<float> keyRect(int n) const noexcept
    {
        const float wk = static_cast<float>(getWidth()) / static_cast<float>(kWhites);
        const float h = static_cast<float>(getHeight());
        if (!isBlack(n)) return {static_cast<float>(whiteIndex(n)) * wk, 0.f, wk, h};
        return {static_cast<float>(whiteIndex(n)) * wk - wk * 0.3f, 0.f, wk * 0.6f, h * 0.62f};
    }

    void drawKey(juce::Graphics& g, int n, int tgt, int det) const
    {
        const auto r = keyRect(n).reduced(0.5f);
        const bool black = isBlack(n);
        juce::Colour fill = black ? juce::Colour(0xff1b1b1f) : juce::Colour(0xffe9e9ed);
        if (tint && quantizer.inScale(n)) fill = black ? juce::Colour(0xff7b3a83) : juce::Colour(0xffddb4e2);
        if (n == tgt) fill = pcui::kAccent;
        g.setColour(fill);
        g.fillRoundedRectangle(r, 2.f);
        g.setColour(juce::Colour(0xff0d0d0f));
        g.drawRoundedRectangle(r, 2.f, 1.f);
        if (n == det) {
            g.setColour(juce::Colours::white);
            g.fillEllipse(r.getCentreX() - 3.f, r.getBottom() - 10.f, 6.f, 6.f);
        }
        if (!black && n % 12 == 0) {
            g.setColour(juce::Colour(0xff55555c));
            g.setFont(juce::Font(juce::FontOptions(10.f)));
            g.drawText("C" + juce::String(n / 12 - 1), r.toNearestInt().withTrimmedTop(juce::jmax(0, getHeight() - 16)),
                       juce::Justification::centredBottom);
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    const std::atomic<float>& detected;
    const std::atomic<float>& target;
    std::atomic<float>* rootValue = nullptr;
    std::atomic<float>* scaleValue = nullptr;
    ScaleQuantizer quantizer;
    bool tint = false;
};
