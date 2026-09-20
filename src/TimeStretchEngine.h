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
        int64_t directInputFrames = 0;
        int64_t timeRatioFallbackEvents = 0;
        int64_t timeRatioFallbackFrames = 0;
        int64_t nonFiniteInputFrames = 0;
    };

    TimeStretchEngine() = default;
    ~TimeStretchEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = spec.numChannels;
        bufferSize = static_cast<int> (spec.maximumBlockSize);
        stretch.presetDefault (numChannels, sampleRate, true);
        stretch.reset();
        scratchBuffer.setSize (numChannels, bufferSize, false, true, true);
        inputPointers.resize (numChannels);
        outputPointers.resize (numChannels);
        bypassDelay.setMaximumDelayInSamples (stretch.outputLatency() + bufferSize);
        bypassDelay.prepare (spec);
        bypassDelay.setDelay (static_cast<float> (stretch.outputLatency()));
        bypassDelay.reset();
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
        timeRatio = std::isfinite (ratio) ? juce::jlimit (0.5f, 2.0f, ratio) : 1.0f;
    }

    void setPitchRatio (float ratio)
    {
        pitchRatio = std::isfinite (ratio) ? juce::jlimit (0.5f, 2.0f, ratio) : 1.0f;
        stretch.setTransposeSemitones (12.0f * std::log2 (pitchRatio));
    }

    void setEnabled (bool shouldProcess) noexcept
    {
        enabled = shouldProcess;
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

        if (diagnosticsEnabled)
        {
            diagnostics.directInputFrames += numSamples;
            diagnostics.engineRequestedInputFrames += numSamples;
            diagnostics.engineConsumedInputFrames += numSamples;
        }

        for (int channel = 0; channel < inputChannels; ++channel)
        {
            auto* scratch = scratchBuffer.getWritePointer (channel);
            const auto* input = buffer.getReadPointer (channel);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                scratch[sample] = input[sample];
                if (diagnosticsEnabled && ! std::isfinite (input[sample]))
                    ++diagnostics.nonFiniteInputFrames;
            }
        }

        for (int channel = inputChannels; channel < numChannels; ++channel)
            scratchBuffer.clear (channel, 0, numSamples);

        for (int channel = 0; channel < inputChannels; ++channel)
        {
            inputPointers[channel] = scratchBuffer.getReadPointer (channel);
            outputPointers[channel] = buffer.getWritePointer (channel);
        }

        for (int channel = 0; channel < inputChannels; ++channel)
        {
            const auto* input = scratchBuffer.getReadPointer (channel);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                bypassDelay.pushSample (channel, input[sample]);
                const auto delayed = bypassDelay.popSample (channel, static_cast<float> (stretch.outputLatency()));
                if (! enabled)
                    buffer.setSample (channel, sample, delayed);
            }
        }

        if (std::abs (timeRatio - 1.0f) > 1.0e-6f && diagnosticsEnabled)
        {
            ++diagnostics.timeRatioFallbackEvents;
            diagnostics.timeRatioFallbackFrames += numSamples;
        }

        if (enabled)
            stretch.process (inputPointers, numSamples, outputPointers, numSamples);

        if (diagnosticsEnabled)
            diagnostics.engineProducedOutputFrames += numSamples;

        for (int channel = inputChannels; channel < buffer.getNumChannels(); ++channel)
            buffer.clear (channel, 0, numSamples);
    }

    int getOutputLatency() const noexcept { return stretch.outputLatency(); }

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch { 42 };
    juce::AudioBuffer<float> scratchBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> bypassDelay;
    std::vector<const float*> inputPointers;
    std::vector<float*> outputPointers;
    double sampleRate = 48000.0;
    int numChannels = 2;
    int bufferSize = 2048;
    float timeRatio = 1.0f;
    float pitchRatio = 1.0f;
    bool enabled = true;
    bool diagnosticsEnabled = false;
    Diagnostics diagnostics;
};
