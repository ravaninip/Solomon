#pragma once
#include <memory>
#include <vector>

#include "PluginProcessor.h"

class SolomonEditor : public juce::AudioProcessorEditor
{
public:
    explicit SolomonEditor (SolomonProcessor&);
    ~SolomonEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    class Look : public juce::LookAndFeel_V4
    {
    public:
        Look();
    };

    Knob& addKnob (const juce::String& paramId, const juce::String& text, const juce::String& tip, bool horizontal = false);
    Choice& addChoice (const juce::String& paramId, const juce::String& text, const juce::String& tip);
    void layoutRow (juce::Rectangle<int> area, std::vector<Knob*> knobs);

    SolomonProcessor& proc;
    Look look;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    Knob *harmonics = nullptr, *timbre = nullptr, *morph = nullptr, *tune = nullptr, *volume = nullptr;
    Knob *cutoff = nullptr, *resonance = nullptr, *lfoRate = nullptr;
    Knob *ampA = nullptr, *ampD = nullptr, *ampS = nullptr, *ampR = nullptr;
    Knob *fltA = nullptr, *fltD = nullptr, *fltS = nullptr, *fltR = nullptr;
    Knob* mod[solomon::kNumModSources][solomon::kNumModDests] = {};
    Choice *model = nullptr, *filterMode = nullptr, *lfoShape = nullptr;

    std::vector<juce::Rectangle<int>> panels;
    juce::Label title, oscHeader, filterHeader, ampHeader, fltEnvHeader, lfoHeader, matrixHeader;
    juce::Label sourceLabels[solomon::kNumModSources], destLabels[solomon::kNumModDests];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SolomonEditor)
};
