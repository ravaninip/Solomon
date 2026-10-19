#pragma once
#include "PluginProcessor.h"

class SolomonEditor : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit SolomonEditor (SolomonProcessor&);
    ~SolomonEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    SolomonProcessor& proc;
    juce::Label title;
    juce::Slider gainSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ComboBox modeBox;
    int seenRevision = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SolomonEditor)
};
