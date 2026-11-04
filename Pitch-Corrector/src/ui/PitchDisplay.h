#pragma once
#include <atomic>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginLookAndFeel.h"

/// Black inset display: large target-note letter inside a dial that lights up
/// from the centre towards how many cents the input is away from the target.
class PitchDisplay : public juce::Component {
public:
    PitchDisplay(const std::atomic<float>& det, const std::atomic<float>& tgt) : detected(det), target(tgt) {}

    void tick()
    {
        const float d = detected.load(std::memory_order_relaxed);
        const float t = target.load(std::memory_order_relaxed);
        voiced = d >= 0.f && t >= 0.f;
        if (voiced) {
            detMidi = d;
            tgtMidi = static_cast<int>(std::lround(t));
            cents += 0.35f * (juce::jlimit(-50.f, 50.f, (d - t) * 100.f) - cents);
        } else {
            cents *= 0.8f;
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        using Palette = pcui::Palette;
        const Palette& P = pcui::pal();
        const auto b = getLocalBounds().toFloat();
        g.setColour(P.well);
        g.fillRoundedRectangle(b, 10.f);
        g.setColour(P.line);
        g.drawRoundedRectangle(b.reduced(0.5f), 10.f, 1.f);

        const auto c = b.getCentre();
        const float r = std::min(b.getWidth(), b.getHeight()) * 0.5f - 12.f;
        if (r < 20.f) return;
        const float halfSpan = juce::MathConstants<float>::pi * 0.66f; // dial spans about 240 degrees

        for (int i = -10; i <= 10; ++i) { // one tick per 5 cents, -50..+50
            const float cv = static_cast<float>(i) * 5.f;
            const float a = static_cast<float>(i) / 10.f * halfSpan;
            const bool lit = voiced && ((cents >= 0.f && cv >= 0.f && cv <= cents + 2.5f)
                                        || (cents < 0.f && cv <= 0.f && cv >= cents - 2.5f));
            const float r0 = r * (i == 0 ? 0.80f : 0.88f);
            const juce::Point<float> p0(c.x + std::sin(a) * r0, c.y - std::cos(a) * r0);
            const juce::Point<float> p1(c.x + std::sin(a) * r, c.y - std::cos(a) * r);
            g.setColour(lit ? P.accent : (i == 0 ? P.dim : P.line));
            g.drawLine({p0, p1}, lit ? 4.f : 2.f);
        }

        const juce::String full = voiced ? noteName(tgtMidi) : juce::String("-");
        const juce::String letter = full.substring(0, 1);
        const juce::String sharp = full.length() > 1 && full[1] == '#' ? "#" : "";
        g.setColour(voiced ? juce::Colours::white : P.line);
        g.setFont(juce::Font(juce::FontOptions(r * 0.95f, juce::Font::bold)));
        const auto letterArea = juce::Rectangle<float>(c.x - r * 0.5f, c.y - r * 0.55f, r, r * 0.95f);
        g.drawText(letter, letterArea, juce::Justification::centred);
        if (sharp.isNotEmpty()) {
            g.setColour(P.accent);
            g.setFont(juce::Font(juce::FontOptions(r * 0.35f, juce::Font::bold)));
            g.drawText(sharp, letterArea.withX(letterArea.getCentreX() + r * 0.22f).withWidth(r * 0.4f)
                                  .withHeight(r * 0.4f),
                       juce::Justification::centred);
        }

        g.setFont(juce::Font(juce::FontOptions(juce::jlimit(11.f, 15.f, r * 0.17f))));
        g.setColour(P.dim);
        const juce::String info = voiced ? "IN " + noteName(juce::roundToInt(detMidi)) + "   "
                                               + (cents >= 0.f ? "+" : "") + juce::String(juce::roundToInt(cents)) + " ct"
                                         : juce::String("NO PITCH");
        g.drawText(info, juce::Rectangle<float>(c.x - r, c.y + r * 0.50f, r * 2.f, r * 0.3f),
                   juce::Justification::centred);
    }

    static juce::String noteName(int midi)
    {
        static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        return juce::String(names[((midi % 12) + 12) % 12]) + juce::String(midi / 12 - 1);
    }

private:
    const std::atomic<float>& detected;
    const std::atomic<float>& target;
    bool voiced = false;
    float detMidi = 0.f, cents = 0.f;
    int tgtMidi = 0;
};
