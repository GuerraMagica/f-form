#include "PluginEditor.h"

PitchTimeProAudioProcessorEditor::PitchTimeProAudioProcessorEditor (PitchTimeProAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p)
{
    setSize (420, 220);

    timeRatioSlider.setRange (0.5, 2.0);
    timeRatioSlider.setValue (1.0);
    timeRatioSlider.setNumDecimalPlacesToDisplay (2);
    timeRatioSlider.setTextValueSuffix ("x");

    pitchRatioSlider.setRange (0.5, 2.0);
    pitchRatioSlider.setValue (1.0);
    pitchRatioSlider.setNumDecimalPlacesToDisplay (2);
    pitchRatioSlider.setTextValueSuffix ("x");

    enabledButton.setButtonText ("Enabled");
    enabledButton.setToggleState (true, juce::dontSendNotification);

    addAndMakeVisible (timeRatioSlider);
    addAndMakeVisible (pitchRatioSlider);
    addAndMakeVisible (enabledButton);

    timeRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getParameters(), "time_ratio", timeRatioSlider);
    pitchRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getParameters(), "pitch_ratio", pitchRatioSlider);
    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getParameters(), "enabled", enabledButton);
}

void PitchTimeProAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawText ("F-Form", getLocalBounds(), juce::Justification::centredTop, true);
}

void PitchTimeProAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    auto sliderArea = area.removeFromTop (80);

    timeRatioSlider.setBounds (sliderArea.removeFromLeft (area.getWidth() / 2).reduced (10, 10));
    pitchRatioSlider.setBounds (sliderArea.removeFromRight (area.getWidth() / 2).reduced (10, 10));
    enabledButton.setBounds (area.removeFromTop (40));
}
