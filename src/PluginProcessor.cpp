#include "PluginProcessor.h"
#include "PluginEditor.h"

PitchTimeProAudioProcessor::PitchTimeProAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", {
          std::make_unique<juce::AudioParameterFloat>("time_ratio", "Time Ratio", 0.5f, 2.0f, 1.0f),
          std::make_unique<juce::AudioParameterFloat>("pitch_ratio", "Pitch Ratio", 0.5f, 2.0f, 1.0f),
          std::make_unique<juce::AudioParameterBool>("enabled", "Enabled", true)
      })
{
}

bool PitchTimeProAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo())
    {
        return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
    }

    return false;
}

void PitchTimeProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (getTotalNumOutputChannels());
    timeStretchEngine.prepare (spec);
    setLatencySamples (timeStretchEngine.getOutputLatency());
}

juce::AudioProcessorEditor* PitchTimeProAudioProcessor::createEditor()
{
    return new PitchTimeProAudioProcessorEditor (*this);
}

void PitchTimeProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    // Parametros del plugin.
    timeRatio = *parameters.getRawParameterValue ("time_ratio");
    pitchRatio = *parameters.getRawParameterValue ("pitch_ratio");
    enabled = *parameters.getRawParameterValue ("enabled") > 0.5f;

    if (! enabled)
    {
        return;
    }

    timeStretchEngine.setTimeRatio (timeRatio);
    timeStretchEngine.setPitchRatio (pitchRatio);
    timeStretchEngine.process (buffer);
}

void PitchTimeProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PitchTimeProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchTimeProAudioProcessor();
}
