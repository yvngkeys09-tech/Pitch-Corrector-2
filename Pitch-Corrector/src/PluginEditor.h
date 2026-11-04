#pragma once
#include <array>
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
        label.setColour(juce::Label::textColourId, pcui::kDim);
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

    PluginLookAndFeel laf; // declared first: outlives every child component
    PitchCorrectorProcessor& proc;

    KnobCtl kAmount, kSpeed, kHumanize, kFlex;
    KnobCtl kNatVib, kRate, kDepth, kVar, kDelay, kRise;

    PitchDisplay display;
    KeyboardDisplay keyboard;

    juce::Label scaleLabel, shapeLabel;
    juce::ComboBox scale, vibShape;
    juce::TextButton bypass{"BYPASS"};
    std::array<juce::TextButton, 12> rootBtns;

    juce::Rectangle<int> titleArea, leftPanel, rightPanel, vibPanel;

    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<CA> scaleA, shapeA;
    std::unique_ptr<BA> bypassA;
};
