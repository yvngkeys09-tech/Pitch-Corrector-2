#include "PluginEditor.h"

PitchCorrectorEditor::PitchCorrectorEditor(PitchCorrectorProcessor& p)
    : AudioProcessorEditor(&p), proc(p), display(p.detectedMidi, p.targetMidi),
      keyboard(p.apvts, p.detectedMidi, p.targetMidi)
{
    setLookAndFeel(&laf);
    auto& vts = proc.apvts;

    kSpeed.init(*this, vts, "speed", "Retune Speed", " %");
    kHumanize.init(*this, vts, "humanize", "Humanize", " %");
    kAmount.init(*this, vts, "amount", "Correction", " %");
    kFlex.init(*this, vts, "flex", "Flex-Tune", " %");
    kNatVib.init(*this, vts, "natvib", "Natural Vib.", " %");
    kRate.init(*this, vts, "vibrate", "Rate", " Hz");
    kDepth.init(*this, vts, "vibdepth", "Depth", " ct");
    kVar.init(*this, vts, "vibvar", "Variation", " %");
    kDelay.init(*this, vts, "vibdelay", "Onset Delay", " ms");
    kRise.init(*this, vts, "vibrise", "Onset Rise", " ms");

    addAndMakeVisible(display);
    addAndMakeVisible(keyboard);

    scale.addItemList(vts.getParameter("scale")->getAllValueStrings(), 1);
    vibShape.addItemList(vts.getParameter("vibshape")->getAllValueStrings(), 1);
    scaleLabel.setText("SCALE", juce::dontSendNotification);
    shapeLabel.setText("SHAPE", juce::dontSendNotification);
    for (auto* l : {&scaleLabel, &shapeLabel}) {
        l->setFont(juce::Font(juce::FontOptions(11.f, juce::Font::bold)));
        l->setColour(juce::Label::textColourId, pcui::kDim);
        addAndMakeVisible(*l);
    }
    shapeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(scale);
    addAndMakeVisible(vibShape);

    bypass.setClickingTogglesState(true);
    addAndMakeVisible(bypass);

    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (int i = 0; i < 12; ++i) {
        auto& b = rootBtns[static_cast<size_t>(i)];
        b.setButtonText(names[i]);
        b.setClickingTogglesState(true);
        b.setRadioGroupId(1001);
        b.onClick = [this, i] {
            if (auto* prm = proc.apvts.getParameter("root")) {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost(prm->convertTo0to1(static_cast<float>(i)));
                prm->endChangeGesture();
            }
        };
        addAndMakeVisible(b);
    }

    scaleA = std::make_unique<CA>(vts, "scale", scale);
    shapeA = std::make_unique<CA>(vts, "vibshape", vibShape);
    bypassA = std::make_unique<BA>(vts, "bypass", bypass);

    setResizable(true, true);
    setResizeLimits(600, 440, 1300, 900);
    setSize(760, 560);
    startTimerHz(30);
}

PitchCorrectorEditor::~PitchCorrectorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void PitchCorrectorEditor::paint(juce::Graphics& g)
{
    using namespace pcui;
    g.fillAll(kBg);
    g.setColour(kPanel);
    for (const auto& r : {leftPanel, rightPanel, vibPanel}) g.fillRoundedRectangle(r.toFloat(), 8.f);

    g.setColour(kText);
    g.setFont(juce::Font(juce::FontOptions(15.f, juce::Font::bold)));
    g.drawText("PITCH-CORRECTOR", titleArea, juce::Justification::centredLeft);

    g.setColour(kDim);
    g.setFont(juce::Font(juce::FontOptions(11.f, juce::Font::bold)));
    g.drawText("VIBRATO", vibPanel.reduced(10, 4).removeFromTop(14), juce::Justification::centredLeft);

    g.setColour(bypass.getToggleState() ? juce::Colour(0xff6b6b72) : juce::Colour(0xff32cd32)); // status LED
    g.fillEllipse(static_cast<float>(bypass.getX() - 18), static_cast<float>(titleArea.getCentreY() - 5), 10.f, 10.f);
}

void PitchCorrectorEditor::resized()
{
    auto r = getLocalBounds().reduced(10);
    auto title = r.removeFromTop(26);
    bypass.setBounds(title.removeFromRight(90).reduced(0, 2));
    titleArea = title;
    r.removeFromTop(6);

    const int H = r.getHeight();
    auto bottom = r.removeFromBottom(juce::jlimit(30, 44, H / 9));
    r.removeFromBottom(6);
    vibPanel = r.removeFromBottom(H * 28 / 100);
    r.removeFromBottom(6);
    auto kb = r.removeFromBottom(juce::jlimit(50, 90, H / 6));
    r.removeFromBottom(6);
    auto top = r;

    scaleLabel.setBounds(bottom.removeFromLeft(48));
    scale.setBounds(bottom.removeFromLeft(130).reduced(0, 4));
    bottom.removeFromLeft(10);
    const int bw = bottom.getWidth() / 12;
    for (auto& b : rootBtns) b.setBounds(bottom.removeFromLeft(bw).reduced(2, 4));

    const int colW = juce::jmax(100, top.getWidth() / 5);
    leftPanel = top.removeFromLeft(colW);
    rightPanel = top.removeFromRight(colW);
    display.setBounds(top.reduced(6, 0));
    keyboard.setBounds(kb);

    auto halves = [](juce::Rectangle<int> p, KnobCtl& a, KnobCtl& b) {
        p = p.reduced(4);
        const int h = p.getHeight() / 2;
        a.place(p.removeFromTop(h));
        b.place(p);
    };
    halves(leftPanel, kSpeed, kHumanize);
    halves(rightPanel, kAmount, kFlex);

    auto inner = vibPanel.reduced(8, 4);
    inner.removeFromTop(14);
    const int cw = inner.getWidth() / 7;
    kNatVib.place(inner.removeFromLeft(cw));
    auto shapeCell = inner.removeFromLeft(cw);
    shapeLabel.setBounds(shapeCell.removeFromTop(16));
    vibShape.setBounds(shapeCell.withSizeKeepingCentre(juce::jmax(40, shapeCell.getWidth() - 12), 24));
    kRate.place(inner.removeFromLeft(cw));
    kDepth.place(inner.removeFromLeft(cw));
    kVar.place(inner.removeFromLeft(cw));
    kDelay.place(inner.removeFromLeft(cw));
    kRise.place(inner.removeFromLeft(cw));
}

void PitchCorrectorEditor::timerCallback()
{
    display.tick();
    keyboard.tick();
    const int rt = juce::jlimit(0, 11, static_cast<int>(proc.apvts.getRawParameterValue("root")->load()));
    auto& b = rootBtns[static_cast<size_t>(rt)];
    if (!b.getToggleState()) b.setToggleState(true, juce::dontSendNotification);
    repaint(bypass.getBounds().expanded(24, 4)); // keep the LED in step with Bypass
}
