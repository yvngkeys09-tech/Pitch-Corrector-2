#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

/// VocalTune-style flat skin: white/dark cards, thin magenta knob arcs with a
/// filled pie centre, pill buttons and bordered dropdowns. All colours come
/// from the active light/dark palette.
class PluginLookAndFeel : public juce::LookAndFeel_V4 {
public:
    PluginLookAndFeel() { applyTheme(); }

    /// Re-resolves every colour against the current palette (light/dark).
    void applyTheme()
    {
        auto& P = pcui::pal();
        setColour(juce::ResizableWindow::backgroundColourId, P.bg);
        setColour(juce::Label::textColourId, P.text);
        setColour(juce::Slider::textBoxTextColourId, P.text);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::backgroundColourId, P.panel);
        setColour(juce::ComboBox::textColourId, P.text);
        setColour(juce::ComboBox::outlineColourId, P.line);
        setColour(juce::ComboBox::arrowColourId, P.dim);
        setColour(juce::PopupMenu::backgroundColourId, P.panel);
        setColour(juce::PopupMenu::textColourId, P.text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, P.accentDim);
        setColour(juce::PopupMenu::highlightedTextColourId, P.accentText);
        setColour(juce::TextButton::buttonColourId, P.panel);
        setColour(juce::TextButton::buttonOnColourId, P.accent);
        setColour(juce::TextButton::textColourOffId, P.dim);
        setColour(juce::TextButton::textColourOnId, P.accentText);
        setColour(juce::ToggleButton::textColourId, P.dim);
        setColour(juce::ToggleButton::tickColourId, P.accent);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1,
                          juce::Slider&) override
    {
        auto& P = pcui::pal();
        const auto b = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                              static_cast<float>(w), static_cast<float>(h)).reduced(3.f);
        const float d = std::min(b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float arcR = d * 0.5f - 1.5f;
        const float bodyR = arcR - 5.f;
        const float ang = a0 + pos * (a1 - a0);

        // Filled pie wedge from 12 o'clock to the current value.
        juce::Path pie;
        pie.addCentredArc(c.x, c.y, bodyR * 0.5f, bodyR * 0.5f, 0.f,
                          -juce::MathConstants<float>::halfPi, ang, true);
        pie.lineTo(c.x, c.y);
        pie.closeSubpath();
        g.setColour(P.accent.withAlpha(0.16f));
        g.fillPath(pie);

        // Thin track + value arc.
        const juce::PathStrokeType stroke(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::Path track, value;
        track.addCentredArc(c.x, c.y, arcR, arcR, 0.f, a0, a1, true);
        value.addCentredArc(c.x, c.y, arcR, arcR, 0.f, a0, ang, true);
        g.setColour(P.line);
        g.strokePath(track, stroke);
        g.setColour(P.accent);
        g.strokePath(value, stroke);

        // Knob body: subtle vertical gradient for a hint of depth ("3DIFY").
        g.setGradientFill(juce::ColourGradient(P.panelSoft.brighter(0.10f), c.x, c.y - bodyR,
                                               P.panelSoft.darker(0.18f), c.x, c.y + bodyR, false));
        g.fillEllipse(c.x - bodyR, c.y - bodyR, bodyR * 2.f, bodyR * 2.f);
        g.setColour(P.line.withAlpha(0.7f));
        g.drawEllipse(c.x - bodyR, c.y - bodyR, bodyR * 2.f, bodyR * 2.f, 1.f);

        // Pointer line.
        juce::Path pointer;
        pointer.addRoundedRectangle(-1.f, -bodyR + 2.5f, 2.f, bodyR * 0.42f, 1.f);
        g.setColour(pos > 0.02f ? P.accent : P.dim);
        g.fillPath(pointer, juce::AffineTransform::rotation(ang).translated(c.x, c.y));
    }

    juce::Button* createSliderButton(juce::Slider&, bool) override { return nullptr; }
};
