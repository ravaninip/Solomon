#pragma once
#include <array>
#include <memory>
#include <vector>

#include "Layout.h"
#include "PluginProcessor.h"
#include "Widgets.h"

class SolomonEditor : public juce::AudioProcessorEditor,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    explicit SolomonEditor (SolomonProcessor&);
    ~SolomonEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // The layouts in use (shared by all instances, like the theme).
    static LayoutManager& layouts();
    // Every element the user can place, with its default position at the base window size, as the
    // "plugin-layout-descriptor" JSON that the Theme Studio reads (see docs/LAYOUTS.md).
    juce::String describeLayout();
    // Where an element is now and whether it is shown (false if there is no such element).
    bool elementInfo (const juce::String& id, juce::Rectangle<int>& bounds, bool& visible) const;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        juce::String paramId;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        juce::String paramId;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    class Look : public juce::LookAndFeel_V4
    {
    public:
        Look();
        void apply();                       // (re)reads the colours of the current theme
        // Bipolar horizontal sliders (the modulation matrix): the bar grows from the centre.
        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                               float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    };

    Knob& addKnob (const juce::String& paramId, const juce::String& text, const juce::String& tip, bool horizontal = false);
    Choice& addChoice (const juce::String& paramId, const juce::String& text, const juce::String& tip);
    void layoutRow (juce::Rectangle<int> area, std::vector<Knob*> knobs);
    void placePresetBar (juce::Rectangle<int> area);

    // Everything the user can move, resize or hide: how to read its default place, put it somewhere else
    // and show or hide it. Ids are part of the layout file format and must not change.
    struct Element
    {
        juce::String id, type, label, param, parent;
        bool locked = false;                 // never hidden, always on top: the controls that switch the layout
        std::function<juce::Rectangle<int>()> current;
        std::function<void (juce::Rectangle<int>)> place;
        std::function<void (bool)> show;
        std::function<bool()> shown;
    };
    void registerElements();
    void addElement (Element e) { elements.push_back (std::move (e)); }
    void addKnobElement (const juce::String& id, const juce::String& parent, Knob& k, const juce::String& label);
    void addMenuElement (const juce::String& id, const juce::String& parent, Choice& c, const juce::String& label, bool hasLabel);
    void addComponentElement (const juce::String& id, const juce::String& type, const juce::String& label,
                              const juce::String& parent, juce::Component& c, bool locked = false);
    void applyLayout();
    void showLayoutMenu();

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;        // the theme changed
    void applyTheme();
    void rebuildThemeMenu();
    void themeMenuChanged();
    void updateModelLabels();
    void rebuildPresetMenu();
    void selectPreset (int entryIndex);
    void stepPreset (int direction);
    void askToSavePreset();
    void say (const juce::String& message);

    float param (const char* id) const;

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

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" };
    juce::Label presetNote;
    juce::ComboBox themeBox;
    juce::TextButton layoutButton { "Layout" };
    std::vector<Element> elements;
    std::array<bool, 6> panelShown { true, true, true, true, true, true };
    bool layoutEnabled = true;
    int shownModel = -1, shownPreset = -2, noteCountdown = 0;

    ui::FilterCurve filterCurve;
    ui::EnvelopeView ampView, fltView;
    ui::VoiceAndLevel voiceMeter;
    std::array<std::unique_ptr<ui::SourceBar>, solomon::kNumModSources> sourceBars;

    std::vector<juce::Rectangle<int>> panels;
    juce::Label title, oscHeader, filterHeader, ampHeader, fltEnvHeader, lfoHeader, matrixHeader;
    juce::Label sourceLabels[solomon::kNumModSources], destLabels[solomon::kNumModDests];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SolomonEditor)
};
