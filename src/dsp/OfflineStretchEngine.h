#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <signalsmith-stretch/signalsmith-stretch.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

struct OfflineStretchRequest
{
    double sampleRate = 48000.0;
    double timeRatio = 1.0;
    double pitchSemitones = 0.0;
    int channels = 1;
    int64_t inputFrames = 0;
    int64_t outputFrames = 0;
    bool splitComputation = true;
};

struct OfflineStretchMetrics
{
    int64_t inputFrames = 0;
    int64_t outputFrames = 0;
    int64_t consumedFrames = 0;
    int64_t producedFrames = 0;
    int64_t inputLatency = 0;
    int64_t outputLatency = 0;
    int64_t prerollFrames = 0;
    int64_t tailFrames = 0;
    bool exactDuration = false;
};

class OfflineStretchEngine
{
public:
    explicit OfflineStretchEngine (long seed = 42) : stretch (seed) {}

    OfflineStretchMetrics render (const OfflineStretchRequest& request,
                                  const juce::AudioBuffer<float>& input,
                                  juce::AudioBuffer<float>& output)
    {
        validate (request, input, output);

        stretch.presetDefault (request.channels, request.sampleRate, request.splitComputation);
        stretch.reset();
        stretch.setTransposeSemitones (static_cast<float> (request.pitchSemitones));

        inputPointers.resize (static_cast<size_t> (request.channels));
        outputPointers.resize (static_cast<size_t> (request.channels));

        for (int channel = 0; channel < request.channels; ++channel)
        {
            inputPointers[static_cast<size_t> (channel)] = input.getReadPointer (channel);
            outputPointers[static_cast<size_t> (channel)] = output.getWritePointer (channel);
        }

        const auto playbackRate = static_cast<float> (request.inputFrames)
                                / static_cast<float> (request.outputFrames);
        const auto seekLength = stretch.outputSeekLength (playbackRate);
        if (request.inputFrames < seekLength)
            throw std::invalid_argument ("Input region is shorter than Signalsmith seek context");

        if (! stretch.exact (inputPointers, static_cast<int> (request.inputFrames),
                             outputPointers, static_cast<int> (request.outputFrames)))
            throw std::runtime_error ("Signalsmith exact render rejected the request");

        OfflineStretchMetrics metrics;
        metrics.inputFrames = request.inputFrames;
        metrics.outputFrames = request.outputFrames;
        metrics.consumedFrames = request.inputFrames;
        metrics.producedFrames = request.outputFrames;
        metrics.inputLatency = stretch.inputLatency();
        metrics.outputLatency = stretch.outputLatency();
        metrics.prerollFrames = seekLength;
        metrics.tailFrames = request.outputFrames - std::max<int64_t> (0, request.outputFrames - metrics.outputLatency);
        metrics.exactDuration = metrics.producedFrames == metrics.outputFrames;
        return metrics;
    }

    OfflineStretchMetrics renderStreaming (juce::AudioFormatReader& reader,
                                           juce::AudioFormatWriter& writer,
                                           const OfflineStretchRequest& request,
                                           int blockFrames)
    {
        if (blockFrames <= 0)
            throw std::invalid_argument ("Streaming block size must be positive");
        if (request.inputFrames != reader.lengthInSamples || request.channels != reader.numChannels)
            throw std::invalid_argument ("Streaming request does not match reader");

        if (! std::isfinite (request.sampleRate) || ! std::isfinite (request.timeRatio)
            || request.timeRatio <= 0.0 || request.outputFrames != static_cast<int64_t> (std::llround (request.inputFrames * request.timeRatio)))
            throw std::invalid_argument ("Invalid streaming frame contract");

        stretch.presetDefault (request.channels, request.sampleRate, request.splitComputation);
        stretch.reset();
        stretch.setTransposeSemitones (static_cast<float> (request.pitchSemitones));

        const auto playbackRate = static_cast<float> (request.inputFrames)
                                / static_cast<float> (request.outputFrames);
        const auto seekLength = stretch.outputSeekLength (playbackRate);
        if (request.inputFrames < seekLength)
            throw std::invalid_argument ("Input region is shorter than Signalsmith seek context");

        const auto prerollOutput = static_cast<int64_t> (seekLength / playbackRate);
        const auto bodyOutputFrames = request.outputFrames - prerollOutput;
        const auto bodyInputFrames = request.inputFrames - seekLength;
        const auto maxInputBlock = std::max (1, static_cast<int> (std::ceil (blockFrames * playbackRate)) + 2);

        juce::AudioBuffer<float> inputBuffer (request.channels, std::max (seekLength, maxInputBlock));
        juce::AudioBuffer<float> outputBuffer (request.channels, blockFrames);
        inputPointers.resize (static_cast<size_t> (request.channels));
        outputPointers.resize (static_cast<size_t> (request.channels));

        if (! reader.read (&inputBuffer, 0, seekLength, 0, true, true))
            throw std::runtime_error ("Could not read Signalsmith pre-roll");
        for (int channel = 0; channel < request.channels; ++channel)
            inputPointers[static_cast<size_t> (channel)] = inputBuffer.getReadPointer (channel);
        stretch.outputSeek (inputPointers, seekLength);

        int64_t inputCursor = seekLength;
        int64_t bodyInputCursor = 0;
        int64_t outputCursor = 0;
        while (outputCursor < bodyOutputFrames)
        {
            const auto outputCount = static_cast<int> (std::min<int64_t> (blockFrames, bodyOutputFrames - outputCursor));
            const auto targetInputCursor = outputCursor + outputCount == bodyOutputFrames
                ? bodyInputFrames
                : static_cast<int64_t> (std::llround ((outputCursor + outputCount) * playbackRate));
            const auto inputCount = static_cast<int> (targetInputCursor - bodyInputCursor);
            if (inputCount < 0 || inputCount > inputBuffer.getNumSamples())
                throw std::runtime_error ("Streaming scheduler produced invalid input block");

            if (inputCount > 0 && ! reader.read (&inputBuffer, 0, inputCount, inputCursor, true, true))
                throw std::runtime_error ("Could not read streaming input block");

            for (int channel = 0; channel < request.channels; ++channel)
            {
                inputPointers[static_cast<size_t> (channel)] = inputBuffer.getReadPointer (channel);
                outputPointers[static_cast<size_t> (channel)] = outputBuffer.getWritePointer (channel);
            }

            stretch.process (inputPointers, inputCount, outputPointers, outputCount);
            if (! writer.writeFromAudioSampleBuffer (outputBuffer, 0, outputCount))
                throw std::runtime_error ("Could not write streaming output block");

            inputCursor += inputCount;
            bodyInputCursor += inputCount;
            outputCursor += outputCount;
        }

        const auto tailFrames = request.outputFrames - bodyOutputFrames;
        if (tailFrames > 0)
        {
            outputBuffer.setSize (request.channels, static_cast<int> (tailFrames), true, true, true);
            for (int channel = 0; channel < request.channels; ++channel)
                outputPointers[static_cast<size_t> (channel)] = outputBuffer.getWritePointer (channel);
            stretch.flush (outputPointers, static_cast<int> (tailFrames), playbackRate);
            if (! writer.writeFromAudioSampleBuffer (outputBuffer, 0, static_cast<int> (tailFrames)))
                throw std::runtime_error ("Could not write streaming tail");
        }

        OfflineStretchMetrics metrics;
        metrics.inputFrames = request.inputFrames;
        metrics.outputFrames = request.outputFrames;
        metrics.consumedFrames = inputCursor;
        metrics.producedFrames = bodyOutputFrames + tailFrames;
        metrics.inputLatency = stretch.inputLatency();
        metrics.outputLatency = stretch.outputLatency();
        metrics.prerollFrames = seekLength;
        metrics.tailFrames = tailFrames;
        metrics.exactDuration = metrics.producedFrames == request.outputFrames
                             && metrics.consumedFrames == request.inputFrames;
        return metrics;
    }

private:
    static void validate (const OfflineStretchRequest& request,
                          const juce::AudioBuffer<float>& input,
                          const juce::AudioBuffer<float>& output)
    {
        if (! std::isfinite (request.sampleRate) || request.sampleRate <= 0.0)
            throw std::invalid_argument ("Invalid sample rate");
        if (! std::isfinite (request.timeRatio) || request.timeRatio <= 0.0)
            throw std::invalid_argument ("Invalid time ratio");
        if (! std::isfinite (request.pitchSemitones))
            throw std::invalid_argument ("Invalid pitch value");
        if (request.channels < 1 || request.channels > 2)
            throw std::invalid_argument ("Offline prototype currently supports mono and stereo");
        if (request.inputFrames <= 0 || request.outputFrames != static_cast<int64_t> (std::llround (request.inputFrames * request.timeRatio)))
            throw std::invalid_argument ("Invalid exact frame contract");
        if (input.getNumChannels() != request.channels || output.getNumChannels() != request.channels
            || input.getNumSamples() != request.inputFrames || output.getNumSamples() != request.outputFrames)
            throw std::invalid_argument ("Buffer dimensions do not match request");
    }

    signalsmith::stretch::SignalsmithStretch<float> stretch;
    std::vector<const float*> inputPointers;
    std::vector<float*> outputPointers;
};