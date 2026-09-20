#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>

class PitchTimeProAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PitchTimeProAudioProcessorEditor (PitchTimeProAudioProcessor&);
    ~PitchTimeProAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PitchTimeProAudioProcessor& audioProcessor;
    juce::Slider timeRatioSlider;
    juce::Slider pitchRatioSlider;
    juce::ToggleButton enabledButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeRatioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchRatioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchTimeProAudioProcessorEditor)
};
