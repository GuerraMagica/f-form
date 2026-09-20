#pragma once

#include <signalsmith-stretch/signalsmith-stretch.h>
#include <JuceHeader.h>
#include <cmath>
#include <vector>

class TimeStretchEngine
{
public:
    struct Diagnostics
    {
        int64_t hostInputFrames = 0;
        int64_t fifoInsertedFrames = 0;
        int64_t engineRequestedInputFrames = 0;
        int64_t engineConsumedInputFrames = 0;
        int64_t engineProducedOutputFrames = 0;
        int64_t fifoHighWaterFrames = 0;
        int64_t fifoRejectedFrames = 0;
        int64_t underflowEvents = 0;
        int64_t underflowFrames = 0;
        int64_t overflowEvents = 0;
        int64_t reportedHostLatencySamples = 0;
        int64_t inputLatencySamples = 0;
        int64_t outputLatencySamples = 0;
    };

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
        diagnostics = {};
        diagnostics.inputLatencySamples = stretch.inputLatency();
        diagnostics.outputLatencySamples = stretch.outputLatency();
    }

    void setDiagnosticsEnabled (bool shouldEnable) noexcept
    {
        diagnosticsEnabled = shouldEnable;
    }

    void setReportedHostLatency (int samples) noexcept
    {
        diagnostics.reportedHostLatencySamples = samples;
    }

    Diagnostics getDiagnostics() const noexcept
    {
        return diagnostics;
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

        if (diagnosticsEnabled)
            diagnostics.hostInputFrames += numSamples;

        if (inputChannels == 0 || numSamples == 0)
        {
            buffer.clear();
            return;
        }

        const auto samplesToStore = juce::jmin (numSamples, fifoCapacity - fifoSamples);
        const auto rejectedSamples = numSamples - samplesToStore;

        if (diagnosticsEnabled)
        {
            diagnostics.fifoInsertedFrames += samplesToStore;
            diagnostics.fifoRejectedFrames += rejectedSamples;
            diagnostics.overflowEvents += rejectedSamples > 0 ? 1 : 0;
        }

        for (int channel = 0; channel < inputChannels; ++channel)
            inputFifo.copyFrom (channel, fifoSamples, buffer.getReadPointer (channel), samplesToStore);

        for (int channel = inputChannels; channel < numChannels; ++channel)
            inputFifo.clear (channel, fifoSamples, samplesToStore);

        fifoSamples += samplesToStore;

        if (diagnosticsEnabled)
            diagnostics.fifoHighWaterFrames = juce::jmax<int64_t> (diagnostics.fifoHighWaterFrames, fifoSamples);

        const auto requestedInputSamples = juce::jmax (1, juce::roundToInt (numSamples / timeRatio));
        const auto inputSamples = juce::jmin (requestedInputSamples, fifoSamples);

        if (diagnosticsEnabled)
        {
            diagnostics.engineRequestedInputFrames += requestedInputSamples;
            diagnostics.engineConsumedInputFrames += inputSamples;
            diagnostics.underflowEvents += inputSamples < requestedInputSamples ? 1 : 0;
            diagnostics.underflowFrames += requestedInputSamples - inputSamples;
        }

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

        if (diagnosticsEnabled)
            diagnostics.engineProducedOutputFrames += numSamples;

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
    bool diagnosticsEnabled = false;
    Diagnostics diagnostics;
};
