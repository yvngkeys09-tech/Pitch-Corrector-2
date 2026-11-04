#pragma once
#include <array>
#include <memory>
#include "PluginProcessor.h"
#include "ui/KeyboardDisplay.h"
#include "ui/PitchDisplay.h"
#include "ui/PluginLookAndFeel.h"

/// A rotary knob with a caption and a parameter attachment.
struct KnobCtl {
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    void init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramId, const juce::String& text, const juce::String& suffix)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
        slider.setTextValueSuffix(suffix);
        label.setText(text.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(juce::FontOptions(11.f, juce::Font::bold)));
        label.setColour(juce::Label::textColourId, pcui::pal().dim);
        parent.addAndMakeVisible(slider);
        parent.addAndMakeVisible(label);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);
    }

    void place(juce::Rectangle<int> r)
    {
        label.setBounds(r.removeFromTop(16));
        slider.setBounds(r);
    }
};

class PitchCorrectorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PitchCorrectorEditor(PitchCorrectorProcessor&);
    ~PitchCorrectorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyTheme(bool dark);          // light/dark switch
    void updateModeButtons();           // HQ vs Low Latency highlight

    PluginLookAndFeel laf; // declared first: outlives every child component
    PitchCorrectorProcessor& proc;

    // --- Correction section -------------------------------------------------
    KnobCtl kAmount, kSpeed, kHumanize, kFlex, kMix;
    // --- Vibrato section ----------------------------------------------------
    KnobCtl kNatVib, kRate, kDepth, kVar, kDelay, kRise;

    PitchDisplay display;
    KeyboardDisplay keyboard;

    juce::Label scaleLabel, formantLabel;
    juce::ComboBox scale, formant;
    juce::TextButton bypass{"BYPASS"};
    juce::TextButton modeHQ{"HQ"}, modeLL{"LOW LATENCY"}, themeBtn{"DARK"};
    std::array<juce::TextButton, 12> rootBtns;

    bool darkMode = true;

    juce::Rectangle<int> titleArea, leftPanel, rightPanel, vibPanel, headerPanel;

    /// Two knobs stacked vertically inside a panel.
    static void halves(juce::Rectangle<int> p, KnobCtl& a, KnobCtl& b)
    {
        p = p.reduced(4);
        const int h = p.getHeight() / 2;
        a.place(p.removeFromTop(h));
        b.place(p);
    }

    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<CA> scaleA, formantA;
    std::unique_ptr<BA> bypassA;
};
