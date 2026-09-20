#pragma once

#include <signalsmith-stretch/signalsmith-stretch.h>
#include <JuceHeader.h>
#include <cmath>
#include <vector>

class TimeStretchEngine
{
public:
    TimeStretchEngine() = default;
    ~TimeStretchEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = spec.numChannels;
        bufferSize = static_cast<int> (spec.maximumBlockSize);
        fifoCapacity = bufferSize * 64;
        inputFifo.setSize (numChannels, fifoCapacity, false, true, true);
        fifoSamples = 0;
        stretch.presetDefault (numChannels, sampleRate, true);
        stretch.reset();
        inputPointers.resize (numChannels);
        outputPointers.resize (numChannels);
    }

    void setTimeRatio (float ratio)
    {
        timeRatio = juce::jlimit (0.5f, 2.0f, ratio);
    }

    void setPitchRatio (float ratio)
    {
        pitchRatio = juce::jlimit (0.5f, 2.0f, ratio);
        stretch.setTransposeSemitones (12.0f * std::log2 (pitchRatio));
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const auto inputChannels = juce::jmin (numChannels, buffer.getNumChannels());
        const auto numSamples = buffer.getNumSamples();

        if (inputChannels == 0 || numSamples == 0)
        {
            buffer.clear();
            return;
        }

        const auto samplesToStore = juce::jmin (numSamples, fifoCapacity - fifoSamples);
        for (int channel = 0; channel < inputChannels; ++channel)
            inputFifo.copyFrom (channel, fifoSamples, buffer.getReadPointer (channel), samplesToStore);

        for (int channel = inputChannels; channel < numChannels; ++channel)
            inputFifo.clear (channel, fifoSamples, samplesToStore);

        fifoSamples += samplesToStore;

        const auto requestedInputSamples = juce::jmax (1, juce::roundToInt (numSamples / timeRatio));
        const auto inputSamples = juce::jmin (requestedInputSamples, fifoSamples);

        if (inputSamples == 0)
        {
            buffer.clear();
            return;
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            inputPointers[channel] = inputFifo.getReadPointer (channel);
            outputPointers[channel] = buffer.getWritePointer (channel);
        }

        stretch.process (inputPointers, inputSamples, outputPointers, numSamples);

        const auto remainingSamples = fifoSamples - inputSamples;
        for (int channel = 0; channel < numChannels; ++channel)
            inputFifo.copyFrom (channel, 0, inputFifo.getReadPointer (channel, inputSamples), remainingSamples);

        fifoSamples = remainingSamples;

        for (int channel = inputChannels; channel < buffer.getNumChannels(); ++channel)
            buffer.clear (channel, 0, numSamples);
    }

    int getOutputLatency() const noexcept { return stretch.outputLatency(); }

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch { 42 };
    juce::AudioBuffer<float> inputFifo;
    std::vector<const float*> inputPointers;
    std::vector<float*> outputPointers;
    double sampleRate = 48000.0;
    int numChannels = 2;
    int bufferSize = 2048;
    int fifoCapacity = 0;
    int fifoSamples = 0;
    float timeRatio = 1.0f;
    float pitchRatio = 1.0f;
};
