#include "TimeStretchEngine.h"

#include <JuceHeader.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

namespace
{
struct Options
{
    juce::File input;
    juce::File output;
    juce::File diagnostics;
    double timeRatio = 1.0;
    double pitchSemitones = 0.0;
    int blockSize = 512;
    bool randomPartitions = false;
    uint32_t seed = 1337;
};

std::string getOption (int argc, char* argv[], const std::string& name, const std::string& fallback = {})
{
    for (int index = 1; index + 1 < argc; ++index)
        if (argv[index] == name)
            return argv[index + 1];

    return fallback;
}

bool hasFlag (int argc, char* argv[], const std::string& name)
{
    for (int index = 1; index < argc; ++index)
        if (argv[index] == name)
            return true;

    return false;
}

Options parseOptions (int argc, char* argv[])
{
    Options options;
    options.input = juce::File (getOption (argc, argv, "--input"));
    options.output = juce::File (getOption (argc, argv, "--output"));
    options.diagnostics = juce::File (getOption (argc, argv, "--diagnostics"));
    options.timeRatio = std::stod (getOption (argc, argv, "--time-ratio", "1.0"));
    options.pitchSemitones = std::stod (getOption (argc, argv, "--pitch-semitones", "0.0"));
    options.blockSize = std::stoi (getOption (argc, argv, "--block-size", "512"));
    options.randomPartitions = hasFlag (argc, argv, "--random-partitions");
    options.seed = static_cast<uint32_t> (std::stoul (getOption (argc, argv, "--seed", "1337")));

    if (options.input == juce::File() || options.output == juce::File() || options.diagnostics == juce::File())
        throw std::invalid_argument ("--input, --output and --diagnostics are required");

    if (! std::isfinite (options.timeRatio) || options.timeRatio < 0.5 || options.timeRatio > 2.0)
        throw std::invalid_argument ("--time-ratio must be finite and in [0.5, 2.0]");

    if (! std::isfinite (options.pitchSemitones))
        throw std::invalid_argument ("--pitch-semitones must be finite");

    if (options.blockSize <= 0)
        throw std::invalid_argument ("--block-size must be positive");

    return options;
}

void writeDiagnostics (const juce::File& file,
                       const Options& options,
                       int64_t inputFrames,
                       int channels,
                       double sampleRate,
                       const TimeStretchEngine::Diagnostics& diagnostics)
{
    file.getParentDirectory().createDirectory();
    std::ofstream stream (file.getFullPathName().toStdString());
    if (! stream)
        throw std::runtime_error ("Could not write diagnostics file");

    stream << "{\n"
           << "  \"mode\": \"realtime_insert_fixed_output\",\n"
           << "  \"input\": \"" << options.input.getFullPathName() << "\",\n"
           << "  \"output\": \"" << options.output.getFullPathName() << "\",\n"
           << "  \"sample_rate\": " << sampleRate << ",\n"
           << "  \"channels\": " << channels << ",\n"
           << "  \"input_frames\": " << inputFrames << ",\n"
           << "  \"time_ratio\": " << options.timeRatio << ",\n"
           << "  \"pitch_semitones\": " << options.pitchSemitones << ",\n"
           << "  \"block_size\": " << options.blockSize << ",\n"
           << "  \"random_partitions\": " << (options.randomPartitions ? "true" : "false") << ",\n"
           << "  \"seed\": " << options.seed << ",\n"
           << "  \"diagnostics\": {\n"
           << "    \"host_input_frames\": " << diagnostics.hostInputFrames << ",\n"
           << "    \"fifo_inserted_frames\": " << diagnostics.fifoInsertedFrames << ",\n"
           << "    \"engine_requested_input_frames\": " << diagnostics.engineRequestedInputFrames << ",\n"
           << "    \"engine_consumed_input_frames\": " << diagnostics.engineConsumedInputFrames << ",\n"
           << "    \"engine_produced_output_frames\": " << diagnostics.engineProducedOutputFrames << ",\n"
           << "    \"fifo_high_water_frames\": " << diagnostics.fifoHighWaterFrames << ",\n"
           << "    \"fifo_rejected_frames\": " << diagnostics.fifoRejectedFrames << ",\n"
           << "    \"underflow_events\": " << diagnostics.underflowEvents << ",\n"
           << "    \"underflow_frames\": " << diagnostics.underflowFrames << ",\n"
           << "    \"overflow_events\": " << diagnostics.overflowEvents << ",\n"
           << "    \"reported_host_latency_samples\": " << diagnostics.reportedHostLatencySamples << ",\n"
           << "    \"input_latency_samples\": " << diagnostics.inputLatencySamples << ",\n"
           << "    \"output_latency_samples\": " << diagnostics.outputLatencySamples << "\n"
           << "  }\n"
           << "}\n";
}
}

int main (int argc, char* argv[])
{
    try
    {
        const auto options = parseOptions (argc, argv);
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (options.input));
        if (reader == nullptr)
            throw std::runtime_error ("Could not read input WAV");

        if (reader->numChannels < 1 || reader->numChannels > 2)
            throw std::runtime_error ("Current realtime harness supports mono and stereo only");

        if (reader->lengthInSamples > std::numeric_limits<int>::max())
            throw std::runtime_error ("Input is too large for the current harness");

        const auto inputFrames = static_cast<int> (reader->lengthInSamples);
        const auto channels = static_cast<int> (reader->numChannels);
        const auto sampleRate = reader->sampleRate;
        juce::AudioBuffer<float> input (channels, inputFrames);
        if (! reader->read (&input, 0, inputFrames, 0, true, true))
            throw std::runtime_error ("Could not decode input WAV");

        juce::AudioBuffer<float> output (channels, inputFrames);
        TimeStretchEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (options.blockSize), static_cast<juce::uint32> (channels) };
        engine.prepare (spec);
        engine.setDiagnosticsEnabled (true);
        engine.setReportedHostLatency (engine.getOutputLatency());
        engine.setTimeRatio (static_cast<float> (options.timeRatio));
        engine.setPitchRatio (static_cast<float> (std::pow (2.0, options.pitchSemitones / 12.0)));

        std::mt19937 random (options.seed);
        std::uniform_int_distribution<int> randomSize (1, options.blockSize);
        int64_t position = 0;
        while (position < inputFrames)
        {
            const auto remaining = inputFrames - static_cast<int> (position);
            const auto requested = options.randomPartitions ? randomSize (random) : options.blockSize;
            const auto blockFrames = juce::jmin (remaining, requested);
            juce::AudioBuffer<float> block (channels, blockFrames);
            for (int channel = 0; channel < channels; ++channel)
                block.copyFrom (channel, 0, input, channel, static_cast<int> (position), blockFrames);

            engine.process (block);
            for (int channel = 0; channel < channels; ++channel)
                output.copyFrom (channel, static_cast<int> (position), block, channel, 0, blockFrames);

            position += blockFrames;
        }

        options.output.getParentDirectory().createDirectory();
        auto outputStream = std::make_unique<juce::FileOutputStream> (options.output);
        if (! outputStream->openedOk())
            throw std::runtime_error ("Could not open output WAV");

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (
            outputStream.release(), sampleRate, static_cast<unsigned int> (channels), 32, {}, 0));
        if (writer == nullptr)
            throw std::runtime_error ("Could not create output WAV writer");

        if (! writer->writeFromAudioSampleBuffer (output, 0, inputFrames))
            throw std::runtime_error ("Could not write output WAV");

        writeDiagnostics (options.diagnostics, options, inputFrames, channels, sampleRate, engine.getDiagnostics());
        std::cout << "PASS: rendered " << inputFrames << " frames to " << options.output.getFullPathName() << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
