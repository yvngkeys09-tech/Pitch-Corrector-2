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

    addFloat("amount", "Snap", 0.f, 100.f, 0.1f, 100.f);
    addFloat("speed", "Retune Speed", 0.f, 100.f, 0.1f, 50.f);
    addFloat("humanize", "Humanize", 0.f, 100.f, 0.1f, 0.f);
    addFloat("flex", "Flex-Tune", 0.f, 100.f, 0.1f, 0.f);

    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterID{"root", 1}, "Key",
        juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterID{"scale", 1}, "Scale",
        juce::StringArray{"Chromatic", "Major", "Minor", "Harmonic Minor", "Pentatonic"}, 0));

    // Vocal-timbre presets (replaces the old "Shape" feature).
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterID{"formant", 1}, "Formant",
        juce::StringArray{"Neutral", "Soprano", "Alto/Tenor", "Low Male", "Instrument", "Bass"}, 0));

    addFloat("mix", "Mix", 0.f, 100.f, 0.1f, 100.f);

    // HQ = full-quality long-window PSOLA; Low Latency = short window for tracking live vocals.
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterID{"quality", 1}, "Quality",
        juce::StringArray{"HQ", "Low Latency"}, 0));

    addFloat("natvib", "Natural Vibrato", 0.f, 200.f, 1.f, 100.f);
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
    pVibRate = apvts.getRawParameterValue("vibrate");
    pVibDepth = apvts.getRawParameterValue("vibdepth");
    pVibVar = apvts.getRawParameterValue("vibvar");
    pVibDelay = apvts.getRawParameterValue("vibdelay");
    pVibRise = apvts.getRawParameterValue("vibrise");
    pRoot = apvts.getRawParameterValue("root");
    pScale = apvts.getRawParameterValue("scale");
    pFormant = apvts.getRawParameterValue("formant");
    pMix = apvts.getRawParameterValue("mix");
    pQuality = apvts.getRawParameterValue("quality");
    pBypass = apvts.getRawParameterValue("bypass");
}

bool PitchCorrectorProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto& o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo())
           && o == l.getMainInputChannelSet();
}

void PitchCorrectorProcessor::applyQualityMode() noexcept
{
    const int mode = juce::jlimit(0, kNumModes - 1, static_cast<int>(pQuality->load()));
    if (mode == curMode || sr <= 0.0) return;
    curMode = mode;
    qualityHQ.store(mode == HQ, std::memory_order_relaxed);

    frame = mode == HQ ? kFrameHq : kFrameLl;
    hop   = mode == HQ ? kHopHq   : kHopLl;

    yin.prepare(sr, frame, kMinHz);
    engine.prepare(static_cast<double>(hop) / sr);

    detRing.assign(static_cast<size_t>(frame), 0.f);
    detFrame.assign(static_cast<size_t>(frame), 0.f);
    detPos = hopCount = 0;

    // In low-latency mode the shifter window is trimmed so the plugin stays
    // usable while monitoring through the DAW.
    const float llMinHz = mode == HQ ? kMinHz : 110.f;
    for (auto& s : shifter) s.prepare(sr, llMinHz, kMaxHz);
    const int latency = shifter[0].getLatency();
    for (auto& d : dryDelay) d.prepare(latency);
    setLatencySamples(latency); // dry path uses exactly the same value

    shiftCur = shiftEnd = shiftStep = 0.f;
    wetGain = 0.f;
    voiced = false;
    periodNow = static_cast<float>(sr / 200.0);
}

void PitchCorrectorProcessor::prepareToPlay(double sampleRate, int maxBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    curMode = -1;                    // force re-configuration below
    applyQualityMode();

    for (auto& f : formant) f.prepare(sr, 1);

    wetSlew = 1.f - std::exp(-1.f / (0.005f * static_cast<float>(sr))); // ~5 ms

    // Keep the host block within what the ring buffer logic expects: the
    // detector tolerates any block size, but a huge block would make the
    // correction ramp sound stepped.
    juce::ignoreUnused(maxBlock);
}

void PitchCorrectorProcessor::releaseResources()
{
    curMode = -1; // everything is rebuilt lazily in the next prepareToPlay
}

void PitchCorrectorProcessor::updateEngineParams() noexcept
{
    engineParams.amount = pAmount->load() * 0.01f;
    engineParams.speed = pSpeed->load() * 0.01f;
    engineParams.humanize = pHumanize->load() * 0.01f;
    engineParams.flex = pFlex->load() * 0.01f;
    engineParams.naturalVibrato = pNatVib->load() * 0.01f;
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
    for (int j = 0; j < frame; ++j) {
        const float v = detRing[static_cast<size_t>((detPos + j) % frame)];
        detFrame[static_cast<size_t>(j)] = v;
        energy += v * v;
    }
    const float rms = std::sqrt(energy / static_cast<float>(frame));
    const auto r = yin.detect(detFrame.data());
    // Slightly higher gate than before: breath and noise used to trigger a
    // bogus pitch read that made the correction jump audibly (the "glitch").
    voiced = r.f0 >= kMinHz && r.f0 <= kMaxHz && r.confidence > 0.15f && rms > 0.003f;

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
    shiftStep = (shiftEnd - shiftCur) / static_cast<float>(hop);
}

void PitchCorrectorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nCh = juce::jmin(buffer.getNumChannels(), 2);
    const int ns = buffer.getNumSamples();
    if (nCh == 0 || ns <= 0) return;

    if (detRing.empty()) prepareToPlay(sr, ns); // defensive: host skipped prepare
    applyQualityMode();                          // reacts to HQ / Low-Latency flips

    updateEngineParams();
    quantizer.set(static_cast<int>(pRoot->load()),
                  static_cast<ScaleQuantizer::Scale>(static_cast<int>(pScale->load())));
    bypassed = pBypass->load() > 0.5f;

    const int preset = juce::jlimit(0, kNumFormants - 1, static_cast<int>(pFormant->load()));
    const float fRatio = kFormantRatio[preset];
    const float mix = pMix->load() * 0.01f;

    float* ch[2] = {buffer.getWritePointer(0), nCh > 1 ? buffer.getWritePointer(1) : nullptr};

    double inEnergy = 0.0, outEnergy = 0.0;

    for (int s = 0; s < ns; ++s) {
        const float in[2] = {ch[0][s], ch[1] ? ch[1][s] : ch[0][s]};

        detRing[static_cast<size_t>(detPos)] = nCh > 1 ? 0.5f * (in[0] + in[1]) : in[0];
        detPos = (detPos + 1) % frame;
        if (++hopCount >= hop) { hopCount = 0; onHop(); }

        shiftCur += shiftStep;
        const float ratio = std::exp2(shiftCur * (1.f / 12.f));

        const float wetTarget = (!bypassed && voiced) ? mix : 0.f;
        wetGain += wetSlew * (wetTarget - wetGain);

        for (int c = 0; c < nCh; ++c) {
            const auto i = static_cast<size_t>(c);
            shifter[i].setParams(periodNow, ratio);
            const float dry = dryDelay[i].process(in[c]);
            float wet = shifter[i].process(in[c]);
            if (preset != Neutral) wet = formant[i].process(0, wet); // per-channel instance, index 0
            const float y = dry + wetGain * (wet - dry);
            ch[c][s] = y;
            inEnergy  += static_cast<double>(in[c]) * in[c];
            outEnergy += static_cast<double>(y) * y;
        }
    }

    // Metering for the GUI (dBFS, smoothed at block rate).
    const float inDb  = static_cast<float>(10.0 * std::log10(inEnergy  / ns + 1e-9));
    const float outDb = static_cast<float>(10.0 * std::log10(outEnergy / ns + 1e-9));
    inLevel.store(inDb, std::memory_order_relaxed);
    outLevel.store(outDb, std::memory_order_relaxed);
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
