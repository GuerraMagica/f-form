#include "OfflineStretchEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
std::string option (int argc, char* argv[], const std::string& name, const std::string& fallback = {})
{
    for (int index = 1; index + 1 < argc; ++index)
        if (argv[index] == name)
            return argv[index + 1];
    return fallback;
}

void writeMetrics (const juce::File& file, const OfflineStretchRequest& request,
                   const OfflineStretchMetrics& metrics, const char* mode)
{
    std::ofstream stream (file.getFullPathName().toStdString());
    if (! stream)
        throw std::runtime_error ("Could not write metrics");

    stream << "{\n"
           << "  \"mode\": \"" << mode << "\",\n"
           << "  \"sample_rate\": " << request.sampleRate << ",\n"
           << "  \"time_ratio\": " << request.timeRatio << ",\n"
           << "  \"pitch_semitones\": " << request.pitchSemitones << ",\n"
           << "  \"channels\": " << request.channels << ",\n"
           << "  \"input_frames\": " << metrics.inputFrames << ",\n"
           << "  \"output_frames\": " << metrics.outputFrames << ",\n"
           << "  \"consumed_frames\": " << metrics.consumedFrames << ",\n"
           << "  \"produced_frames\": " << metrics.producedFrames << ",\n"
           << "  \"input_latency\": " << metrics.inputLatency << ",\n"
           << "  \"output_latency\": " << metrics.outputLatency << ",\n"
           << "  \"preroll_frames\": " << metrics.prerollFrames << ",\n"
           << "  \"tail_frames\": " << metrics.tailFrames << ",\n"
           << "  \"exact_duration\": " << (metrics.exactDuration ? "true" : "false") << "\n"
           << "}\n";
}
}

int main (int argc, char* argv[])
{
    try
    {
        const juce::File inputFile (option (argc, argv, "--input"));
        const juce::File outputFile (option (argc, argv, "--output"));
        const juce::File metricsFile (option (argc, argv, "--metrics"));
        const auto mode = option (argc, argv, "--mode", "exact");
        const auto timeRatio = std::stod (option (argc, argv, "--time-ratio", "1.0"));
        const auto pitchSemitones = std::stod (option (argc, argv, "--pitch-semitones", "0.0"));
        const auto blockFrames = std::stoi (option (argc, argv, "--block-size", "4096"));

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (inputFile));
        if (reader == nullptr)
            throw std::runtime_error ("Could not open input WAV");
        if (reader->numChannels < 1 || reader->numChannels > 2)
            throw std::runtime_error ("Offline prototype currently supports mono and stereo");

        const auto inputFrames = static_cast<int64_t> (reader->lengthInSamples);
        const auto outputFrames = static_cast<int64_t> (std::llround (inputFrames * timeRatio));

        OfflineStretchRequest request;
        request.sampleRate = reader->sampleRate;
        request.timeRatio = timeRatio;
        request.pitchSemitones = pitchSemitones;
        request.channels = static_cast<int> (reader->numChannels);
        request.inputFrames = inputFrames;
        request.outputFrames = outputFrames;

        if (mode == "streaming")
        {
            outputFile.getParentDirectory().createDirectory();
            auto outputStream = std::make_unique<juce::FileOutputStream> (outputFile);
            if (! outputStream->openedOk())
                throw std::runtime_error ("Could not open streaming output");

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (
                outputStream.release(), reader->sampleRate, reader->numChannels, 32, {}, 0));
            if (writer == nullptr)
                throw std::runtime_error ("Could not create streaming WAV writer");

            OfflineStretchEngine engine;
            const auto metrics = engine.renderStreaming (*reader, *writer, request, blockFrames);
            metricsFile.getParentDirectory().createDirectory();
            writeMetrics (metricsFile, request, metrics, "offline_signalsmith_streaming");
            std::cout << "PASS: streaming offline render " << inputFrames << " -> " << outputFrames << " frames\n";
            return 0;
        }

        if (mode != "exact")
            throw std::invalid_argument ("--mode must be exact or streaming");

        juce::AudioBuffer<float> input (static_cast<int> (reader->numChannels), static_cast<int> (inputFrames));
        juce::AudioBuffer<float> output (static_cast<int> (reader->numChannels), static_cast<int> (outputFrames));
        if (! reader->read (&input, 0, static_cast<int> (inputFrames), 0, true, true))
            throw std::runtime_error ("Could not read input WAV");

        OfflineStretchEngine engine;
        const auto metrics = engine.render (request, input, output);

        outputFile.getParentDirectory().createDirectory();
        auto outputStream = std::make_unique<juce::FileOutputStream> (outputFile);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (
            outputStream.release(), reader->sampleRate, reader->numChannels, 32, {}, 0));
        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (output, 0, static_cast<int> (outputFrames)))
            throw std::runtime_error ("Could not write output WAV");

        metricsFile.getParentDirectory().createDirectory();
        writeMetrics (metricsFile, request, metrics, "offline_signalsmith_exact");
        std::cout << "PASS: offline render " << inputFrames << " -> " << outputFrames << " frames\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}