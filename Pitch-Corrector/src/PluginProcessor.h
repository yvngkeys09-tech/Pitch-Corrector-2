#pragma once
#include <array>
#include <atomic>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/CorrectionEngine.h"
#include "dsp/DelayLine.h"
#include "dsp/PsolaShifter.h"
#include "dsp/ScaleQuantizer.h"
#include "dsp/YinDetector.h"

class PitchCorrectorProcessor : public juce::AudioProcessor {
public:
    PitchCorrectorProcessor();

    const juce::String getName() const override { return "Pitch-Corrector"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    bool isBusesLayoutSupported(const BusesLayout& l) const override;
    void prepareToPlay(double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    // Lock-free values for the GUI (MIDI note, -1 = none).
    std::atomic<float> detectedMidi{-1.f}, targetMidi{-1.f};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateEngineParams() noexcept;
    void onHop() noexcept;

    static constexpr int kFrame = 2048, kHop = 512;
    static constexpr float kMinHz = 80.f, kMaxHz = 1000.f;

    YinDetector yin;
    ScaleQuantizer quantizer;
    CorrectionEngine engine;
    CorrectionEngine::Params engineParams;
    std::array<PsolaShifter, 2> shifter;
    std::array<DelayLine, 2> dryDelay;

    std::vector<float> detRing, detFrame;
    int detPos = 0, hopCount = 0;

    double sr = 44100.0;
    float periodNow = 220.f;           // input pitch period in samples (smoothed)
    float shiftCur = 0.f, shiftEnd = 0.f, shiftStep = 0.f; // semitones, ramped per sample
    float wetGain = 0.f, wetSlew = 0.01f;
    bool voiced = false, bypassed = false;

    std::atomic<float>* pAmount = nullptr;
    std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pHumanize = nullptr;
    std::atomic<float>* pFlex = nullptr;
    std::atomic<float>* pNatVib = nullptr;
    std::atomic<float>* pVibShape = nullptr;
    std::atomic<float>* pVibRate = nullptr;
    std::atomic<float>* pVibDepth = nullptr;
    std::atomic<float>* pVibVar = nullptr;
    std::atomic<float>* pVibDelay = nullptr;
    std::atomic<float>* pVibRise = nullptr;
    std::atomic<float>* pRoot = nullptr;
    std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pBypass = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchCorrectorProcessor)
};
