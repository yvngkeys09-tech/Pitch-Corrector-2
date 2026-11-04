#include "PluginEditor.h"

namespace {
/// Sets a TextButton up as a pill-style toggle that reflects the "quality"
/// parameter (HQ / Low Latency) like a segmented control.
void stylePill(juce::TextButton& b)
{
    b.setClickingTogglesState(false);
    b.setFont(juce::Font(juce::FontOptions(11.f, juce::Font::bold)));
    b.setColour(juce::TextButton::buttonColourId, pcui::pal().panelSoft);
    b.setColour(juce::TextButton::textColourOffId, pcui::pal().dim);
}

void setParam01(juce::AudioProcessorValueTreeState& vts, const juce::String& id, float value)
{
    if (auto* prm = vts.getParameter(id)) {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost(prm->convertTo0to1(value));
        prm->endChangeGesture();
    }
}
} // namespace

PitchCorrectorEditor::PitchCorrectorEditor(PitchCorrectorProcessor& p)
    : AudioProcessorEditor(&p), proc(p), display(p.detectedMidi, p.targetMidi),
      keyboard(p.apvts, p.detectedMidi, p.targetMidi)
{
    setLookAndFeel(&laf);
    auto& vts = proc.apvts;

    // ---- Correction knobs --------------------------------------------------
    kAmount.init(*this, vts, "amount",   "Snap",         " %");
    kSpeed.init(*this, vts, "speed",    "Retune Speed",  " %");
    kHumanize.init(*this, vts, "humanize","Humanize",    " %");
    kFlex.init(*this, vts, "flex",      "Flex-Tune",     " %");
    kMix.init(*this, vts, "mix",        "Mix",           " %");

    // ---- Vibrato knobs -----------------------------------------------------
    kNatVib.init(*this, vts, "natvib",  "Natural Vib.", " %");
    kRate.init(*this, vts, "vibrate",   "Rate",         " Hz");
    kDepth.init(*this, vts, "vibdepth", "Depth",        " ct");
    kVar.init(*this, vts, "vibvar",     "Variation",    " %");
    kDelay.init(*this, vts, "vibdelay", "Onset Delay",  " ms");
    kRise.init(*this, vts, "vibrise",   "Onset Rise",   " ms");

    addAndMakeVisible(display);
    addAndMakeVisible(keyboard);

    // ---- Scale + Formant dropdowns -----------------------------------------
    scale.addItemList(vts.getParameter("scale")->getAllValueStrings(), 1);
    formant.addItemList(vts.getParameter("formant")->getAllValueStrings(), 1);
    scaleLabel.setText("SCALE", juce::dontSendNotification);
    formantLabel.setText("FORMANT", juce::dontSendNotification);
    for (auto* l : {&scaleLabel, &formantLabel}) {
        l->setFont(juce::Font(juce::FontOptions(11.f, juce::Font::bold)));
        l->setColour(juce::Label::textColourId, pcui::pal().dim);
        addAndMakeVisible(*l);
    }
    addAndMakeVisible(scale);
    addAndMakeVisible(formant);

    // ---- Transport-style buttons --------------------------------------------
    bypass.setClickingTogglesState(true);
    addAndMakeVisible(bypass);

    stylePill(modeHQ);
    stylePill(modeLL);
    modeHQ.onClick = [this] { setParam01(proc.apvts, "quality", 0.f); updateModeButtons(); };
    modeLL.onClick = [this] { setParam01(proc.apvts, "quality", 1.f); updateModeButtons(); };
    addAndMakeVisible(modeHQ);
    addAndMakeVisible(modeLL);

    themeBtn.setClickingTogglesState(false);
    themeBtn.onClick = [this] { applyTheme(!darkMode); };
    addAndMakeVisible(themeBtn);

    // ---- Root-key buttons ----------------------------------------------------
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (int i = 0; i < 12; ++i) {
        auto& b = rootBtns[static_cast<size_t>(i)];
        b.setButtonText(names[i]);
        b.setClickingTogglesState(true);
        b.setRadioGroupId(1001);
        b.onClick = [this, i] { setParam01(proc.apvts, "root", static_cast<float>(i)); };
        addAndMakeVisible(b);
    }

    scaleA = std::make_unique<CA>(vts, "scale", scale);
    formantA = std::make_unique<CA>(vts, "formant", formant);
    bypassA = std::make_unique<BA>(vts, "bypass", bypass);

    updateModeButtons();

    setResizable(true, true);
    setResizeLimits(640, 480, 1400, 980);
    setSize(820, 600);
    startTimerHz(30);
}

PitchCorrectorEditor::~PitchCorrectorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void PitchCorrectorEditor::applyTheme(bool dark)
{
    darkMode = dark;
    pcui::setPalette(dark);
    laf.applyTheme();
    themeBtn.setButtonText(dark ? "LIGHT" : "DARK"); // shows the *other* mode
    for (auto* l : {&scaleLabel, &formantLabel})
        l->setColour(juce::Label::textColourId, pcui::pal().dim);
    stylePill(modeHQ);
    stylePill(modeLL);
    updateModeButtons();
    repaint();
}

void PitchCorrectorEditor::updateModeButtons()
{
    const int mode = juce::jlimit(0, 1, static_cast<int>(proc.apvts.getRawParameterValue("quality")->load()));
    auto& P = pcui::pal();
    const auto on  = P.accent;
    const auto off = P.panelSoft;
    modeHQ.setColour(juce::TextButton::buttonColourId, mode == PitchCorrectorProcessor::HQ ? on : off);
    modeHQ.setColour(juce::TextButton::textColourOnId, P.accentText);
    modeHQ.setColour(juce::TextButton::textColourOffId, P.dim);
    modeHQ.setToggleState(mode == PitchCorrectorProcessor::HQ, juce::dontSendNotification);
    modeLL.setColour(juce::TextButton::buttonColourId, mode == PitchCorrectorProcessor::LowLatency ? on : off);
    modeLL.setColour(juce::TextButton::textColourOnId, P.accentText);
    modeLL.setColour(juce::TextButton::textColourOffId, P.dim);
    modeLL.setToggleState(mode == PitchCorrectorProcessor::LowLatency, juce::dontSendNotification);
}

void PitchCorrectorEditor::paint(juce::Graphics& g)
{
    auto& P = pcui::pal();
    g.fillAll(P.bg);

    // Raised cards with a hairline border and soft drop shadow ("3D" look).
    for (const auto& r : {leftPanel, rightPanel, vibPanel, headerPanel}) {
        if (r.isEmpty()) continue;
        g.setColour(P.black.withAlpha(0.35f));
        g.fillRoundedRectangle(r.toFloat().translated(0.f, 2.f), 8.f);
        g.setColour(P.panel);
        g.fillRoundedRectangle(r.toFloat(), 8.f);
        g.setColour(P.line.withAlpha(0.6f));
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 8.f, 1.f);
    }

    g.setColour(P.text);
    g.setFont(juce::Font(juce::FontOptions(15.f, juce::Font::bold)));
    g.drawText("PITCH-CORRECTOR", titleArea, juce::Justification::centredLeft);

    g.setColour(P.dim);
    g.setFont(juce::Font(juce::FontOptions(11.f, juce::Font::bold)));
    g.drawText("CORRECTION", leftPanel.reduced(10, 4).removeFromTop(14), juce::Justification::centredLeft);
    g.drawText("TIMBRE & VIBRATO", rightPanel.reduced(10, 4).removeFromTop(14), juce::Justification::centredLeft);
    g.drawText("VIBRATO ONSET", vibPanel.reduced(10, 4).removeFromTop(14), juce::Justification::centredLeft);

    // Status LED next to Bypass: green = active, grey = bypassed.
    g.setColour(bypass.getToggleState() ? juce::Colour(0xff6b6b72) : P.led);
    g.fillEllipse(static_cast<float>(bypass.getX() - 18), static_cast<float>(titleArea.getCentreY() - 5), 10.f, 10.f);
}

void PitchCorrectorEditor::resized()
{
    auto r = getLocalBounds().reduced(10);
    headerPanel = r.removeFromTop(34);
    auto title = headerPanel.reduced(2, 4);
    bypass.setBounds(title.removeFromRight(90));
    themeBtn.setBounds(title.removeFromRight(80).reduced(4, 0));
    auto modes = title.removeFromRight(190).reduced(4, 2);
    const int mw = modes.getWidth() / 2;
    modeHQ.setBounds(modes.removeFromLeft(mw).reduced(0, 0, 2, 0));
    modeLL.setBounds(modes);
    titleArea = title;
    r.removeFromTop(6);

    const int H = r.getHeight();
    auto bottom = r.removeFromBottom(juce::jlimit(30, 44, H / 9));
    r.removeFromBottom(6);
    vibPanel = r.removeFromBottom(H * 26 / 100);
    r.removeFromBottom(6);
    auto kb = r.removeFromBottom(juce::jlimit(50, 90, H / 6));
    r.removeFromBottom(6);
    auto top = r;

    // Bottom strip: SCALE | FORMANT | key row
    scaleLabel.setBounds(bottom.removeFromLeft(48));
    scale.setBounds(bottom.removeFromLeft(120).reduced(0, 4));
    bottom.removeFromLeft(8);
    formantLabel.setBounds(bottom.removeFromLeft(62));
    formant.setBounds(bottom.removeFromLeft(120).reduced(0, 4));
    bottom.removeFromLeft(10);
    const int bw = bottom.getWidth() / 12;
    for (auto& b : rootBtns) b.setBounds(bottom.removeFromLeft(bw).reduced(2, 4));

    // Middle: pitch dial in the centre, knob columns left/right.
    const int colW = juce::jmax(120, top.getWidth() / 5);
    leftPanel = top.removeFromLeft(colW);
    rightPanel = top.removeFromRight(colW);
    display.setBounds(top.reduced(6, 0));
    keyboard.setBounds(kb);

    halves(leftPanel, kSpeed, kHumanize);
    halves(rightPanel, kAmount, kFlex);

    auto inner = vibPanel.reduced(8, 4);
    inner.removeFromTop(16);
    const int cw = inner.getWidth() / 7;
    kNatVib.place(inner.removeFromLeft(cw));
    kMix.place(inner.removeFromLeft(cw));
    kRate.place(inner.removeFromLeft(cw));
    kDepth.place(inner.removeFromLeft(cw));
    kVar.place(inner.removeFromLeft(cw));
    kDelay.place(inner.removeFromLeft(cw));
    kRise.place(inner);
}

void PitchCorrectorEditor::timerCallback()
{
    display.tick();
    keyboard.tick();
    const int rt = juce::jlimit(0, 11, static_cast<int>(proc.apvts.getRawParameterValue("root")->load()));
    auto& b = rootBtns[static_cast<size_t>(rt)];
    if (!b.getToggleState()) b.setToggleState(true, juce::dontSendNotification);
    updateModeButtons();
    repaint(bypass.getBounds().expanded(24, 4)); // keep the LED in step with Bypass
}
