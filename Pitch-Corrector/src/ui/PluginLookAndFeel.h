#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace pcui {
inline const juce::Colour kBg{0xff25252a}, kPanel{0xff34343a}, kDark{0xff131316}, kLine{0xff4a4a52},
    kText{0xffdcdce0}, kDim{0xff8d8d96}, kAccent{0xffc741cf}, kAccentDim{0xff6b2f70};
}

/// Dark hardware-style skin: brushed-metal knobs with a magenta value arc,
/// black inset displays, magenta highlights.
class PluginLookAndFeel : public juce::LookAndFeel_V4 {
public:
    PluginLookAndFeel()
    {
        using namespace pcui;
        setColour(juce::ResizableWindow::backgroundColourId, kBg);
        setColour(juce::Label::textColourId, kText);
        setColour(juce::Slider::textBoxTextColourId, kText);
        setColour(juce::Slider::textBoxBackgroundColourId, kDark);
        setColour(juce::Slider::textBoxOutlineColourId, kLine);
        setColour(juce::ComboBox::backgroundColourId, kDark);
        setColour(juce::ComboBox::textColourId, kText);
        setColour(juce::ComboBox::outlineColourId, kLine);
        setColour(juce::ComboBox::arrowColourId, kAccent);
        setColour(juce::PopupMenu::backgroundColourId, kPanel);
        setColour(juce::PopupMenu::textColourId, kText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccentDim);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextButton::buttonColourId, kDark);
        setColour(juce::TextButton::buttonOnColourId, kAccent);
        setColour(juce::TextButton::textColourOffId, kDim);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1,
                          juce::Slider&) override
    {
        using namespace pcui;
        const auto b = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                              static_cast<float>(w), static_cast<float>(h)).reduced(4.f);
        const float d = std::min(b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float arcR = d * 0.5f - 2.f;
        const float knobR = d * 0.5f * 0.72f;
        const float ang = a0 + pos * (a1 - a0);

        const juce::PathStrokeType stroke(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::Path track, value;
        track.addCentredArc(c.x, c.y, arcR, arcR, 0.f, a0, a1, true);
        value.addCentredArc(c.x, c.y, arcR, arcR, 0.f, a0, ang, true);
        g.setColour(kLine);
        g.strokePath(track, stroke);
        g.setColour(kAccent);
        g.strokePath(value, stroke);

        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff2f2f4), c.x, c.y - knobR,
                                               juce::Colour(0xff77777f), c.x, c.y + knobR, false));
        g.fillEllipse(c.x - knobR, c.y - knobR, knobR * 2.f, knobR * 2.f);
        g.setColour(juce::Colour(0xff0e0e10));
        g.drawEllipse(c.x - knobR, c.y - knobR, knobR * 2.f, knobR * 2.f, 1.5f);

        juce::Path pointer;
        pointer.addRoundedRectangle(-1.5f, -knobR + 3.f, 3.f, knobR * 0.45f, 1.5f);
        g.setColour(juce::Colour(0xff17171a));
        g.fillPath(pointer, juce::AffineTransform::rotation(ang).translated(c.x, c.y));
    }
};
