#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout PitchCorrectorProcessor::createLayout()
{
    using juce::NormalisableRange;
    using juce::ParameterID;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto addFloat = [&p](const char* id, const char* name, float lo, float hi, float step, float def) {
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            ParameterID{id, 1}, name, NormalisableRange<float>(lo, hi, step), def));
    };

    addFloat("amount", "Correction", 0.f, 100.f, 0.1f, 100.f);
    addFloat("speed", "Retune Speed", 0.f, 100.f, 0.1f, 50.f);
    addFloat("humanize", "Humanize", 0.f, 100.f, 0.1f, 0.f);
    addFloat("flex", "Flex-Tune", 0.f, 100.f, 0.1f, 0.f);

    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterID{"root", 1}, "Key",
        juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterID{"scale", 1}, "Scale",
        juce::StringArray{"Chromatic", "Major", "Minor", "Harmonic Minor", "Pentatonic"}, 0));

    addFloat("natvib", "Natural Vibrato", 0.f, 200.f, 1.f, 100.f);
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterID{"vibshape", 1}, "Vibrato Shape",
        juce::StringArray{"Sine", "Triangle", "Square", "Saw"}, 0));
    addFloat("vibrate", "Vibrato Rate", 0.5f, 10.f, 0.1f, 5.5f);
    addFloat("vibdepth", "Vibrato Depth", 0.f, 100.f, 1.f, 0.f);
    addFloat("vibvar", "Vibrato Variation", 0.f, 100.f, 1.f, 25.f);
    addFloat("vibdelay", "Vibrato Onset Delay", 0.f, 1500.f, 1.f, 300.f);
    addFloat("vibrise", "Vibrato Onset Rise", 0.f, 1500.f, 1.f, 300.f);

    p.push_back(std::make_unique<juce::AudioParameterBool>(ParameterID{"bypass", 1}, "Bypass", false));
    return {p.begin(), p.end()};
}

PitchCorrectorProcessor::PitchCorrectorProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createLayout())
{
    pAmount = apvts.getRawParameterValue("amount");
    pSpeed = apvts.getRawParameterValue("speed");
    pHumanize = apvts.getRawParameterValue("humanize");
    pFlex = apvts.getRawParameterValue("flex");
    pNatVib = apvts.getRawParameterValue("natvib");
    pVibShape = apvts.getRawParameterValue("vibshape");
    pVibRate = apvts.getRawParameterValue("vibrate");
    pVibDepth = apvts.getRawParameterValue("vibdepth");
    pVibVar = apvts.getRawParameterValue("vibvar");
    pVibDelay = apvts.getRawParameterValue("vibdelay");
    pVibRise = apvts.getRawParameterValue("vibrise");
    pRoot = apvts.getRawParameterValue("root");
    pScale = apvts.getRawParameterValue("scale");
    pBypass = apvts.getRawParameterValue("bypass");
}

bool PitchCorrectorProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto& o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo())
           && o == l.getMainInputChannelSet();
}

void PitchCorrectorProcessor::prepareToPlay(double sampleRate, int)
{
    sr = sampleRate;
    yin.prepare(sr, kFrame, kMinHz);
    for (auto& s : shifter) s.prepare(sr, kMinHz, kMaxHz);
    const int latency = shifter[0].getLatency();
    for (auto& d : dryDelay) d.prepare(latency);
    setLatencySamples(latency); // dry path uses exactly the same value

    engine.prepare(static_cast<double>(kHop) / sr);

    detRing.assign(kFrame, 0.f);
    detFrame.assign(kFrame, 0.f);
    detPos = hopCount = 0;
    periodNow = static_cast<float>(sr / 200.0);
    shiftCur = shiftEnd = shiftStep = 0.f;
    wetGain = 0.f;
    voiced = false;
    wetSlew = 1.f - std::exp(-1.f / (0.005f * static_cast<float>(sr))); // ~5 ms
}

void PitchCorrectorProcessor::updateEngineParams() noexcept
{
    engineParams.amount = pAmount->load() * 0.01f;
    engineParams.speed = pSpeed->load() * 0.01f;
    engineParams.humanize = pHumanize->load() * 0.01f;
    engineParams.flex = pFlex->load() * 0.01f;
    engineParams.naturalVibrato = pNatVib->load() * 0.01f;
    engineParams.vibShape = static_cast<int>(pVibShape->load());
    engineParams.vibRateHz = pVibRate->load();
    engineParams.vibDepthCents = pVibDepth->load();
    engineParams.vibVariation = pVibVar->load() * 0.01f;
    engineParams.vibDelaySec = pVibDelay->load() * 0.001f;
    engineParams.vibRiseSec = pVibRise->load() * 0.001f;
}

void PitchCorrectorProcessor::onHop() noexcept
{
    // Linearise the ring, oldest sample first.
    float energy = 0.f;
    for (int j = 0; j < kFrame; ++j) {
        const float v = detRing[static_cast<size_t>((detPos + j) % kFrame)];
        detFrame[static_cast<size_t>(j)] = v;
        energy += v * v;
    }
    const float rms = std::sqrt(energy / kFrame);
    const auto r = yin.detect(detFrame.data());
    voiced = r.f0 >= kMinHz && r.f0 <= kMaxHz && rms > 0.002f;

    if (voiced) {
        const float midi = ScaleQuantizer::hzToMidi(r.f0);
        const auto o = engine.update(midi, quantizer, engineParams);
        shiftEnd = o.shiftSemis;

        const float period = static_cast<float>(sr) / r.f0;
        periodNow = periodNow <= 0.f ? period : periodNow + 0.5f * (period - periodNow);
        detectedMidi.store(midi, std::memory_order_relaxed);
        targetMidi.store(static_cast<float>(o.targetNote), std::memory_order_relaxed);
    } else {
        if (engine.unvoiced()) { // long gap: forget the note
            shiftEnd = shiftCur = 0.f;
            periodNow = static_cast<float>(sr / 200.0);
        }
        detectedMidi.store(-1.f, std::memory_order_relaxed);
        targetMidi.store(-1.f, std::memory_order_relaxed);
    }
    // Linear ramp of the shift across the next hop (no zipper steps).
    shiftStep = (shiftEnd - shiftCur) / static_cast<float>(kHop);
}

void PitchCorrectorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nCh = juce::jmin(buffer.getNumChannels(), 2);
    const int ns = buffer.getNumSamples();
    if (nCh == 0) return;

    updateEngineParams();
    quantizer.set(static_cast<int>(pRoot->load()),
                  static_cast<ScaleQuantizer::Scale>(static_cast<int>(pScale->load())));
    bypassed = pBypass->load() > 0.5f;

    float* ch[2] = {buffer.getWritePointer(0), nCh > 1 ? buffer.getWritePointer(1) : nullptr};

    for (int s = 0; s < ns; ++s) {
        const float in[2] = {ch[0][s], ch[1] ? ch[1][s] : ch[0][s]};

        detRing[static_cast<size_t>(detPos)] = nCh > 1 ? 0.5f * (in[0] + in[1]) : in[0];
        detPos = (detPos + 1) % kFrame;
        if (++hopCount >= kHop) { hopCount = 0; onHop(); }

        shiftCur += shiftStep;
        const float ratio = std::exp2(shiftCur * (1.f / 12.f));

        const float wetTarget = (!bypassed && voiced) ? 1.f : 0.f;
        wetGain += wetSlew * (wetTarget - wetGain);

        for (int c = 0; c < nCh; ++c) {
            shifter[static_cast<size_t>(c)].setParams(periodNow, ratio);
            const float dry = dryDelay[static_cast<size_t>(c)].process(in[c]);
            const float wet = shifter[static_cast<size_t>(c)].process(in[c]);
            ch[c][s] = dry + wetGain * (wet - dry);
        }
    }
}

void PitchCorrectorProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void PitchCorrectorProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* PitchCorrectorProcessor::createEditor()
{
    return new PitchCorrectorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PitchCorrectorProcessor(); }
