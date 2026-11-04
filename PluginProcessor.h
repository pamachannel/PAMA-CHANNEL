#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "VoiceEngine.h"

class TopvoxProcessor : public juce::AudioProcessor
{
public:
    TopvoxProcessor();
    ~TopvoxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TOPVOX AI"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void applyPreset (int index);
    int getCurrentPreset() const { return currentPreset.load(); }
    static juce::StringArray presetNames();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct PRef
    {
        std::atomic<float> *autoMix, *autoEq, *autoGain, *inGain, *gate, *eqLow, *eqMid, *eqAir, *reson, *comp,
            *deess, *sat, *revMix, *revSize, *dlyMix, *dlyTime, *dlyFb, *limit, *outGain;
    } pr {};

    tv::VoiceEngine engine;
    juce::Reverb reverb;
    std::atomic<int> currentPreset { 0 };
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopvoxProcessor)
};
