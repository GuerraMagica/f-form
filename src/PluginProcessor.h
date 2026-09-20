#pragma once

#include "TimeStretchEngine.h"
#include <JuceHeader.h>

class PitchTimeProAudioProcessor : public juce::AudioProcessor
{
public:
    PitchTimeProAudioProcessor();
    ~PitchTimeProAudioProcessor() override = default;

    const juce::String getName() const override { return "F-Form"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }

private:
    juce::AudioProcessorValueTreeState parameters;
    float timeRatio = 1.0f;
    float pitchRatio = 1.0f;
    bool enabled = true;
    TimeStretchEngine timeStretchEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchTimeProAudioProcessor)
};
