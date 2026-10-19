#include "PluginEditor.h"

using namespace solomon;

namespace
{
const Theme& T() { return ui::theme(); }

constexpr int kThemeFolderItem = 1000, kThemeReloadItem = 1001;
constexpr int kBaseWidth = 980, kBaseHeight = 720;

void styleHeader (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, T().accent);
    l.setJustificationType (juce::Justification::centredLeft);
}
} // namespace

SolomonEditor::Look::Look() { apply(); }

void SolomonEditor::Look::apply()
{
    const auto& t = T();
    setColour (juce::Slider::rotarySliderFillColourId, t.accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, t.edge);
    setColour (juce::Slider::thumbColourId, t.text);
    setColour (juce::Slider::textBoxTextColourId, t.text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, t.dim);
    setColour (juce::ComboBox::backgroundColourId, t.control);
    setColour (juce::ComboBox::textColourId, t.text);
    setColour (juce::ComboBox::outlineColourId, t.controlEdge);
    setColour (juce::ComboBox::arrowColourId, t.accent);
    setColour (juce::PopupMenu::backgroundColourId, t.control);
    setColour (juce::PopupMenu::textColourId, t.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, t.text);
    setColour (juce::TextButton::buttonColourId, t.control);
    setColour (juce::TextButton::textColourOffId, t.text);
    setColour (juce::TooltipWindow::backgroundColourId, t.control);
    setColour (juce::TooltipWindow::textColourId, t.text);
    setColour (juce::AlertWindow::backgroundColourId, t.panel);
    setColour (juce::AlertWindow::textColourId, t.text);
    setColour (juce::TextEditor::backgroundColourId, t.well);
    setColour (juce::TextEditor::textColourId, t.text);
}

void SolomonEditor::Look::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                            float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                                            juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }
    const float cy = (float) y + (float) height * 0.5f;
    const float centre = (float) x + (float) width * 0.5f;       // matrix ranges are symmetric: 0 sits in the middle
    juce::Rectangle<float> track ((float) x, cy - 3.0f, (float) width, 6.0f);
    g.setColour (T().well);
    g.fillRoundedRectangle (track, 3.0f);

    const bool active = std::abs (sliderPos - centre) > 1.0f;
    g.setColour (sliderPos >= centre ? T().accent : T().warm);
    g.fillRect (juce::Rectangle<float> (juce::jmin (centre, sliderPos), cy - 3.0f, std::abs (sliderPos - centre), 6.0f));
    g.setColour (T().dim);
    g.drawVerticalLine ((int) centre, cy - 6.0f, cy + 6.0f);
    g.setColour (active ? T().text : T().dim);
    g.fillEllipse (sliderPos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
}

namespace
{
// Human-readable value text per parameter kind, and the inverse for typing a value in the text box.
juce::String formatValue (const juce::String& id, double v)
{
    if (id == "cutoff")
        return v < 1000.0 ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0, 2) + " kHz";
    if (id.endsWith ("A") || id.endsWith ("D") || id.endsWith ("R"))
        if (id.startsWith ("amp") || id.startsWith ("flt"))
            return v < 1.0 ? juce::String (juce::roundToInt (v * 1000.0)) + " ms" : juce::String (v, 2) + " s";
    if (id == "lfoRate")   return juce::String (v, 2) + " Hz";
    if (id == "tune")      return (v >= 0 ? "+" : "") + juce::String (v, 1) + " st";
    if (id.startsWith ("mod_")) return (v >= 0 ? "+" : "") + juce::String (v, 2);
    return juce::String (v, 2);
}

double parseValue (const juce::String& text)
{
    auto t = text.trim().toLowerCase();
    double v = t.getDoubleValue();
    if (t.contains ("khz") || (t.endsWithChar ('k'))) v *= 1000.0;
    else if (t.contains ("ms")) v /= 1000.0;
    return v;
}
} // namespace

SolomonEditor::Knob& SolomonEditor::addKnob (const juce::String& paramId, const juce::String& text, const juce::String& tip, bool horizontal)
{
    auto k = std::make_unique<Knob>();
    k->paramId = paramId;
    if (horizontal)
    {
        k->slider.setSliderStyle (juce::Slider::LinearHorizontal);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    }
    else
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k->slider.setTooltip (tip);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (11.5f));
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);
    if (auto* prm = proc.apvts.getParameter (paramId))
        k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    k->slider.textFromValueFunction = [paramId] (double v) { return formatValue (paramId, v); };
    k->slider.valueFromTextFunction = [] (const juce::String& t) { return parseValue (t); };
    k->slider.updateText();
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

SolomonEditor::Choice& SolomonEditor::addChoice (const juce::String& paramId, const juce::String& text, const juce::String& tip)
{
    auto c = std::make_unique<Choice>();
    c->paramId = paramId;
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        c->box.addItemList (prm->choices, 1);
    c->box.setTooltip (tip);
    c->label.setText (text, juce::dontSendNotification);
    c->label.setFont (juce::FontOptions (11.5f));
    c->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, c->box);
    addAndMakeVisible (c->box);
    addAndMakeVisible (c->label);
    choices.push_back (std::move (c));
    return *choices.back();
}


float SolomonEditor::param (const char* id) const
{
    return proc.apvts.getRawParameterValue (id)->load();
}

SolomonEditor::SolomonEditor (SolomonProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);

    title.setText ("Solomon", juce::dontSendNotification);
    title.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, T().accent);
    addAndMakeVisible (title);

    model = &addChoice ("model", "Oscillator model", "Sound generator. Harmonics, Timbre and Morph change meaning with the model.");
    filterMode = &addChoice ("filterMode", "Filter", "Filter type");
    lfoShape = &addChoice ("lfoShape", "LFO shape", "Waveform of the LFO");

    harmonics = &addKnob ("harmonics", "Harmonics", "");
    timbre = &addKnob ("timbre", "Timbre", "");
    morph = &addKnob ("morph", "Morph", "");
    tune = &addKnob ("tune", "Tune", "Pitch offset in semitones");
    volume = &addKnob ("volume", "Volume", "Output level");
    cutoff = &addKnob ("cutoff", "Cutoff", "Filter cut-off frequency (the bright curve shows the modulated value)");
    resonance = &addKnob ("resonance", "Resonance", "Filter resonance");
    lfoRate = &addKnob ("lfoRate", "Rate", "LFO speed");

    ampA = &addKnob ("ampA", "A", "Amp attack");   ampD = &addKnob ("ampD", "D", "Amp decay");
    ampS = &addKnob ("ampS", "S", "Amp sustain");   ampR = &addKnob ("ampR", "R", "Amp release");
    fltA = &addKnob ("fltA", "A", "Filter envelope attack");   fltD = &addKnob ("fltD", "D", "Filter envelope decay");
    fltS = &addKnob ("fltS", "S", "Filter envelope sustain");  fltR = &addKnob ("fltR", "R", "Filter envelope release");

    static const char* srcNames[kNumModSources] = { "LFO", "Envelope", "Velocity", "Pressure" };
    static const char* dstNames[kNumModDests] = { "Timbre", "Morph", "Harmonics", "Cutoff" };
    for (int s = 0; s < kNumModSources; ++s)
        for (int d = 0; d < kNumModDests; ++d)
            mod[s][d] = &addKnob (SolomonProcessor::modId (s, d), "",
                                  juce::String (srcNames[s]) + " to " + dstNames[d] + " (0 = off, double-click to reset)", true);

    styleHeader (oscHeader, "OSCILLATOR");     styleHeader (filterHeader, "FILTER");
    styleHeader (ampHeader, "AMP ENVELOPE");   styleHeader (fltEnvHeader, "FILTER ENVELOPE");
    styleHeader (lfoHeader, "LFO");            styleHeader (matrixHeader, "MODULATION MATRIX   (the bars on the left show each source live; aftertouch and mod wheel feed Pressure)");
    for (auto* l : { &oscHeader, &filterHeader, &ampHeader, &fltEnvHeader, &lfoHeader, &matrixHeader })
        addAndMakeVisible (l);
    for (int s = 0; s < kNumModSources; ++s)
    {
        sourceLabels[s].setText (srcNames[s], juce::dontSendNotification);
        sourceLabels[s].setJustificationType (juce::Justification::centredLeft);
        sourceLabels[s].setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (sourceLabels[s]);
        sourceBars[(size_t) s] = std::make_unique<ui::SourceBar> (s == kModLfo);
        addAndMakeVisible (*sourceBars[(size_t) s]);
    }
    for (int d = 0; d < kNumModDests; ++d)
    {
        destLabels[d].setText (dstNames[d], juce::dontSendNotification);
        destLabels[d].setJustificationType (juce::Justification::centred);
        destLabels[d].setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (destLabels[d]);
    }

    filterCurve.settings = [this] { return ui::FilterCurve::Settings { param ("cutoff"), param ("resonance"), (int) param ("filterMode") }; };
    ampView.adsr = [this] { return ui::EnvelopeView::Adsr { param ("ampA"), param ("ampD"), param ("ampS"), param ("ampR") }; };
    fltView.adsr = [this] { return ui::EnvelopeView::Adsr { param ("fltA"), param ("fltD"), param ("fltS"), param ("fltR") }; };
    for (auto* c : std::initializer_list<juce::Component*> { &filterCurve, &ampView, &fltView, &voiceMeter })
        addAndMakeVisible (c);
    filterCurve.setTooltip ("Filter response. Dim curve: the cut-off you set. Bright curve: the cut-off now, after modulation.");
    voiceMeter.setTooltip ("Sounding voices and output level");

    // presets
    proc.presets.refresh();
    presetBox.setTextWhenNothingSelected ("Sound");
    presetBox.onChange = [this] { selectPreset (presetBox.getSelectedId() - 1); };
    presetBox.setTooltip ("Choose a sound. Presets are starting points: edit them and save your own.");
    prevButton.setTooltip ("Previous preset");  prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.setTooltip ("Next preset");      nextButton.onClick = [this] { stepPreset (+1); };
    saveButton.setTooltip ("Save the current sound as a user preset");
    saveButton.onClick = [this] { askToSavePreset(); };
    presetNote.setFont (juce::FontOptions (11.0f));
    presetNote.setColour (juce::Label::textColourId, T().dim);
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevButton, &nextButton, &saveButton, &presetNote })
        addAndMakeVisible (c);
    rebuildPresetMenu();

    themeBox.setTooltip ("Colour theme. Add your own by saving a theme file (made with the Theme Studio) in the themes folder.");
    themeBox.onChange = [this] { themeMenuChanged(); };
    addAndMakeVisible (themeBox);
    rebuildThemeMenu();
    applyTheme();
    ThemeManager::shared().addChangeListener (this);

    layoutButton.setTooltip ("Layout of the interface. Make your own with the Theme Studio.");
    layoutButton.onClick = [this] { showLayoutMenu(); };
    addAndMakeVisible (layoutButton);
    layouts().addChangeListener (this);
    registerElements();

    updateModelLabels();

    setResizable (true, true);
    setResizeLimits (kBaseWidth / 2, kBaseHeight / 2, kBaseWidth * 2, kBaseHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) kBaseWidth / kBaseHeight);
    setSize (kBaseWidth, kBaseHeight);
    startTimerHz (30);
}

SolomonEditor::~SolomonEditor()
{
    stopTimer();
    ThemeManager::shared().removeChangeListener (this);
    layouts().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void SolomonEditor::paint (juce::Graphics& g)
{
    g.fillAll (T().background);
    g.setColour (T().panel);
    for (size_t i = 0; i < panels.size(); ++i)
        if (i >= panelShown.size() || panelShown[i])
            g.fillRoundedRectangle (panels[i].toFloat(), T().panelRadius);
}

// ------------------------------------------------------------------------ themes
void SolomonEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &layouts())
    {
        resized();
        repaint();
        return;
    }
    applyTheme();
    rebuildThemeMenu();
}

void SolomonEditor::applyTheme()
{
    look.apply();
    title.setColour (juce::Label::textColourId, T().accent);
    presetNote.setColour (juce::Label::textColourId, T().dim);
    for (auto* l : { &oscHeader, &filterHeader, &ampHeader, &fltEnvHeader, &lfoHeader, &matrixHeader })
        l->setColour (juce::Label::textColourId, T().accent);
    sendLookAndFeelChange();
    repaint();
}

void SolomonEditor::rebuildThemeMenu()
{
    auto& themes = ThemeManager::shared();
    themeBox.clear (juce::dontSendNotification);
    themeBox.addSectionHeading ("Themes");
    for (int i = 0; i < (int) themes.themes().size(); ++i)
    {
        if (i == themes.numBuiltIn())
            themeBox.addSectionHeading ("Your themes");
        themeBox.addItem (themes.themes()[(size_t) i].name, i + 1);
    }
    themeBox.addSeparator();
    themeBox.addItem ("Open themes folder...", kThemeFolderItem);
    themeBox.addItem ("Reload themes", kThemeReloadItem);
    themeBox.setSelectedId (themes.indexOf (themes.current().name) + 1, juce::dontSendNotification);
}

void SolomonEditor::themeMenuChanged()
{
    auto& themes = ThemeManager::shared();
    const int id = themeBox.getSelectedId();
    if (id == kThemeFolderItem)
    {
        themes.folder().createDirectory();
        themes.folder().revealToUser();
    }
    else if (id == kThemeReloadItem)
    {
        themes.refresh();
        say (themes.problems().isEmpty() ? "Themes reloaded"
                                         : "Ignored: " + themes.problems()[0]);
        themes.sendSynchronousChangeMessage();       // also refreshes every other open editor
        return;
    }
    else if (id > 0)
    {
        themes.select (themes.themes()[(size_t) (id - 1)].name);
        return;
    }
    rebuildThemeMenu();                              // back to showing the current theme
}

// ------------------------------------------------------------------------ live updates
void SolomonEditor::updateModelLabels()
{
    shownModel = juce::jlimit (0, solomon::kNumModels - 1, (int) param ("model"));
    Knob* macro[3] = { harmonics, timbre, morph };
    for (int c = 0; c < 3; ++c)
    {
        const auto& info = solomon::modelControl (shownModel, c);
        macro[c]->label.setText (info.name, juce::dontSendNotification);
        macro[c]->slider.setTooltip (juce::String (info.hint) + " (" + solomon::modelName (shownModel) + ")");
    }
}

void SolomonEditor::timerCallback()
{
    if ((int) param ("model") != shownModel)
        updateModelLabels();

    const auto& m = proc.meters;
    filterCurve.setLiveCutoff (m.cutoffHz.load (std::memory_order_relaxed));
    filterCurve.repaint();
    voiceMeter.set (m.voiceMask.load (std::memory_order_relaxed), m.peak.load (std::memory_order_relaxed));
    sourceBars[kModLfo]->set (m.lfo.load (std::memory_order_relaxed));
    sourceBars[kModEnv]->set (m.env.load (std::memory_order_relaxed));
    sourceBars[kModVelocity]->set (m.velocity.load (std::memory_order_relaxed));
    sourceBars[kModPressure]->set (m.pressure.load (std::memory_order_relaxed));
    ampView.repaint();
    fltView.repaint();

    // keep the preset box in step with the session (the host can reload the state at any time)
    const int current = proc.presets.indexOfCurrent();
    if (current != shownPreset)
        rebuildPresetMenu();
    if (noteCountdown > 0 && --noteCountdown == 0)
        presetNote.setText ({}, juce::dontSendNotification);
}

// ------------------------------------------------------------------------ presets
void SolomonEditor::say (const juce::String& message)
{
    presetNote.setText (message, juce::dontSendNotification);
    noteCountdown = 30 * 4;
}

void SolomonEditor::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& list = proc.presets.entries();
    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < proc.presets.numFactory(); ++i)
        presetBox.addItem (list[(size_t) i].name, i + 1);
    if ((int) list.size() > proc.presets.numFactory())
    {
        presetBox.addSectionHeading ("User");
        for (int i = proc.presets.numFactory(); i < (int) list.size(); ++i)
            presetBox.addItem (list[(size_t) i].name, i + 1);
    }

    shownPreset = proc.presets.indexOfCurrent();
    if (shownPreset >= 0)
        presetBox.setSelectedId (shownPreset + 1, juce::dontSendNotification);
    else
    {
        presetBox.setSelectedId (0, juce::dontSendNotification);
        const auto name = proc.presets.currentName();
        presetBox.setTextWhenNothingSelected (name.isEmpty() ? juce::String ("Sound") : name);
    }
}

void SolomonEditor::selectPreset (int entryIndex)
{
    if (entryIndex < 0) return;
    if (! proc.presets.load (entryIndex))
        say ("Could not load that preset");
    updateModelLabels();
    shownPreset = -2;                        // refresh the box on the next tick
}

void SolomonEditor::stepPreset (int direction)
{
    const int n = (int) proc.presets.entries().size();
    if (n == 0) return;
    const int current = proc.presets.indexOfCurrent();
    selectPreset (current < 0 ? (direction > 0 ? 0 : n - 1) : (current + direction + n) % n);
}

void SolomonEditor::askToSavePreset()
{
    auto* window = new juce::AlertWindow ("Save preset",
                                          "Name of the preset. A user preset with the same name is replaced.\n"
                                          "Saved in: " + proc.presets.folder().getFullPathName(),
                                          juce::MessageBoxIconType::NoIcon, this);
    const auto current = proc.presets.currentName();
    window->addTextEditor ("name", current.isEmpty() ? juce::String ("My sound") : current);
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<SolomonEditor> safe (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        const auto name = window->getTextEditorContents ("name");
        if (safe->proc.presets.save (name))
            safe->say ("Saved \"" + PresetManager::cleanName (name) + "\"");
        else
            safe->say ("Could not save (empty or reserved name, or folder not writable)");
        safe->shownPreset = -2;
    }), true);
}

// ------------------------------------------------------------------------ user layouts
LayoutManager& SolomonEditor::layouts()
{
    static LayoutManager instance ("Solomon", kBaseWidth, kBaseHeight,
                                   ThemeManager::defaultFolder().getSiblingFile ("Layouts"),
                                   ThemeManager::defaultFolder().getSiblingFile ("layout.txt"));
    return instance;
}

void SolomonEditor::placePresetBar (juce::Rectangle<int> area)
{
    auto line = area.removeFromTop (32);
    prevButton.setBounds (line.removeFromLeft (30));
    line.removeFromLeft (4);
    nextButton.setBounds (line.removeFromRight (30));
    line.removeFromRight (4);
    saveButton.setBounds (line.removeFromRight (62));
    line.removeFromRight (4);
    presetBox.setBounds (line);
    presetNote.setBounds (area.removeFromTop (18));
}

void SolomonEditor::addKnobElement (const juce::String& id, const juce::String& parent, Knob& k, const juce::String& label)
{
    const bool hasLabel = ! k.slider.isHorizontal();          // rotary knobs have a name above them
    Element e;
    e.id = id; e.type = k.slider.isHorizontal() ? "slider" : "knob"; e.label = label; e.param = k.paramId; e.parent = parent;
    e.current = [&k, hasLabel] { return hasLabel ? k.label.getBounds().getUnion (k.slider.getBounds().expanded (2)) : k.slider.getBounds(); };
    e.place = [&k, hasLabel] (juce::Rectangle<int> r)
    {
        if (hasLabel) { k.label.setBounds (r.removeFromTop (16)); k.slider.setBounds (r.reduced (2)); }
        else          { k.slider.setBounds (r); }
    };
    e.show = [&k] (bool b) { k.slider.setVisible (b); k.label.setVisible (b); };
    e.shown = [&k] { return k.slider.isVisible(); };
    addElement (std::move (e));
}

void SolomonEditor::addMenuElement (const juce::String& id, const juce::String& parent, Choice& c, const juce::String& label, bool hasLabel)
{
    Element e;
    e.id = id; e.type = "menu"; e.label = label; e.param = c.paramId; e.parent = parent;
    e.current = [&c, hasLabel] { return hasLabel ? c.label.getBounds().getUnion (c.box.getBounds()) : c.box.getBounds(); };
    e.place = [&c, hasLabel] (juce::Rectangle<int> r)
    {
        if (hasLabel) c.label.setBounds (r.removeFromTop (16));
        c.box.setBounds (r);
    };
    e.show = [&c, hasLabel] (bool b) { c.box.setVisible (b); if (hasLabel) c.label.setVisible (b); };
    e.shown = [&c] { return c.box.isVisible(); };
    addElement (std::move (e));
}

void SolomonEditor::addComponentElement (const juce::String& id, const juce::String& type, const juce::String& label,
                                         const juce::String& parent, juce::Component& c, bool locked)
{
    Element e;
    e.id = id; e.type = type; e.label = label; e.parent = parent; e.locked = locked;
    e.current = [&c] { return c.getBounds(); };
    e.place = [&c] (juce::Rectangle<int> r) { c.setBounds (r); };
    e.show = [&c] (bool b) { c.setVisible (b); };
    e.shown = [&c] { return c.isVisible(); };
    addElement (std::move (e));
}

void SolomonEditor::registerElements()
{
    // panels, in the order resized() builds them
    static const char* panelIds[6] = { "panel.osc", "panel.filter", "panel.lfo", "panel.ampEnv", "panel.fltEnv", "panel.matrix" };
    static const char* panelNames[6] = { "Oscillator", "Filter", "LFO", "Amp envelope", "Filter envelope", "Modulation matrix" };
    for (size_t i = 0; i < 6; ++i)
    {
        Element e;
        e.id = panelIds[i]; e.type = "panel"; e.label = panelNames[i];
        e.current = [this, i] { return i < panels.size() ? panels[i] : juce::Rectangle<int>(); };
        e.place = [this, i] (juce::Rectangle<int> r) { if (i < panels.size()) panels[i] = r; };
        e.show = [this, i] (bool b) { panelShown[i] = b; };
        e.shown = [this, i] { return (bool) panelShown[i]; };
        addElement (std::move (e));
    }

    // top bar
    addComponentElement ("title", "label", "Title", "", title);
    addComponentElement ("theme", "menu", "Theme", "", themeBox, true);
    addComponentElement ("layout", "menu", "Layout", "", layoutButton, true);
    {
        Element e;
        e.id = "preset"; e.type = "group"; e.label = "Presets";
        e.current = [this] { return prevButton.getBounds().getUnion (nextButton.getBounds()).getUnion (presetBox.getBounds())
                                        .getUnion (saveButton.getBounds()).getUnion (presetNote.getBounds()); };
        e.place = [this] (juce::Rectangle<int> r) { placePresetBar (r); };
        e.show = [this] (bool b) { for (auto* c : std::initializer_list<juce::Component*> { &prevButton, &nextButton, &presetBox, &saveButton, &presetNote }) c->setVisible (b); };
        e.shown = [this] { return presetBox.isVisible(); };
        addElement (std::move (e));
    }
    addMenuElement ("model", "", *model, "Oscillator model", true);
    addComponentElement ("meter", "meter", "Voices and level", "", voiceMeter);
    addKnobElement ("knob.volume", "", *volume, "Volume");

    // oscillator
    addComponentElement ("header.osc", "label", "Oscillator header", "panel.osc", oscHeader);
    addKnobElement ("knob.harmonics", "panel.osc", *harmonics, "Harmonics");
    addKnobElement ("knob.timbre", "panel.osc", *timbre, "Timbre");
    addKnobElement ("knob.morph", "panel.osc", *morph, "Morph");
    addKnobElement ("knob.tune", "panel.osc", *tune, "Tune");

    // filter
    addComponentElement ("header.filter", "label", "Filter header", "panel.filter", filterHeader);
    addMenuElement ("menu.filterMode", "panel.filter", *filterMode, "Filter type", false);
    addComponentElement ("graph.filter", "graph", "Filter response", "panel.filter", filterCurve);
    addKnobElement ("knob.cutoff", "panel.filter", *cutoff, "Cutoff");
    addKnobElement ("knob.resonance", "panel.filter", *resonance, "Resonance");

    // LFO
    addComponentElement ("header.lfo", "label", "LFO header", "panel.lfo", lfoHeader);
    addMenuElement ("menu.lfoShape", "panel.lfo", *lfoShape, "LFO shape", false);
    addKnobElement ("knob.lfoRate", "panel.lfo", *lfoRate, "LFO rate");

    // envelopes
    addComponentElement ("header.ampEnv", "label", "Amp envelope header", "panel.ampEnv", ampHeader);
    addComponentElement ("graph.ampEnv", "graph", "Amp envelope shape", "panel.ampEnv", ampView);
    addComponentElement ("header.fltEnv", "label", "Filter envelope header", "panel.fltEnv", fltEnvHeader);
    addComponentElement ("graph.fltEnv", "graph", "Filter envelope shape", "panel.fltEnv", fltView);
    {
        Knob* amp[4] = { ampA, ampD, ampS, ampR };
        Knob* flt[4] = { fltA, fltD, fltS, fltR };
        static const char* stage[4] = { "A", "D", "S", "R" };
        for (int i = 0; i < 4; ++i)
        {
            addKnobElement ("knob.amp" + juce::String (stage[i]), "panel.ampEnv", *amp[i], "Amp " + juce::String (stage[i]));
            addKnobElement ("knob.flt" + juce::String (stage[i]), "panel.fltEnv", *flt[i], "Filter " + juce::String (stage[i]));
        }
    }

    // modulation matrix
    static const char* srcNames[kNumModSources] = { "LFO", "Envelope", "Velocity", "Pressure" };
    static const char* dstNames[kNumModDests] = { "Timbre", "Morph", "Harmonics", "Cutoff" };
    addComponentElement ("header.matrix", "label", "Matrix header", "panel.matrix", matrixHeader);
    for (int s = 0; s < kNumModSources; ++s)
    {
        addComponentElement ("matrix.src." + juce::String (s), "label", srcNames[s], "panel.matrix", sourceLabels[s]);
        addComponentElement ("matrix.bar." + juce::String (s), "meter", juce::String (srcNames[s]) + " level", "panel.matrix", *sourceBars[(size_t) s]);
    }
    for (int d = 0; d < kNumModDests; ++d)
        addComponentElement ("matrix.dst." + juce::String (d), "label", dstNames[d], "panel.matrix", destLabels[d]);
    for (int s = 0; s < kNumModSources; ++s)
        for (int d = 0; d < kNumModDests; ++d)
            addKnobElement ("matrix.cell." + juce::String (s) + "." + juce::String (d), "panel.matrix", *mod[s][d],
                            juce::String (srcNames[s]) + " to " + dstNames[d]);
}

void SolomonEditor::applyLayout()
{
    const auto& layout = layouts().current();
    const double scale = (double) getWidth() / kBaseWidth;
    for (auto& e : elements)
    {
        const auto it = layoutEnabled ? layout.entries.find (e.id) : layout.entries.end();
        if (it == layout.entries.end())
        {
            e.show (true);                                      // not mentioned: the built-in design decides
            continue;
        }
        const auto& b = it->second.bounds;
        e.place (juce::Rectangle<float> ((float) (b.getX() * scale), (float) (b.getY() * scale),
                                         (float) (b.getWidth() * scale), (float) (b.getHeight() * scale)).toNearestInt());
        e.show (e.locked || it->second.visible);
    }
    themeBox.toFront (false);                                   // whatever the layout does, these two stay usable
    layoutButton.toFront (false);
    repaint();
}

bool SolomonEditor::elementInfo (const juce::String& id, juce::Rectangle<int>& bounds, bool& visible) const
{
    for (const auto& e : elements)
        if (e.id == id) { bounds = e.current(); visible = e.shown(); return true; }
    return false;
}

juce::String SolomonEditor::describeLayout()
{
    layoutEnabled = false;
    resized();
    const double scale = (double) kBaseWidth / (double) getWidth();
    juce::Array<juce::var> list;
    for (const auto& e : elements)
    {
        const auto b = e.current();
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", e.id);
        o->setProperty ("type", e.type);
        o->setProperty ("label", e.label);
        if (e.param.isNotEmpty())  o->setProperty ("param", e.param);
        if (e.parent.isNotEmpty()) o->setProperty ("parent", e.parent);
        if (e.locked)              o->setProperty ("locked", true);
        o->setProperty ("x", juce::roundToInt (b.getX() * scale));
        o->setProperty ("y", juce::roundToInt (b.getY() * scale));
        o->setProperty ("w", juce::roundToInt (b.getWidth() * scale));
        o->setProperty ("h", juce::roundToInt (b.getHeight() * scale));
        list.add (juce::var (o));
    }
    layoutEnabled = true;
    resized();

    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "plugin-layout-descriptor");
    root->setProperty ("version", 1);
    root->setProperty ("plugin", "Solomon");
    root->setProperty ("baseWidth", kBaseWidth);
    root->setProperty ("baseHeight", kBaseHeight);
    root->setProperty ("elements", list);
    return juce::JSON::toString (juce::var (root), false);
}

void SolomonEditor::showLayoutMenu()
{
    auto& lm = layouts();
    juce::PopupMenu menu;
    menu.addSectionHeader ("Layout");
    const int current = lm.indexOf (lm.current().name);
    for (int i = 0; i < (int) lm.layouts().size(); ++i)
        menu.addItem (i + 1, lm.layouts()[(size_t) i].name, true, i == current);
    menu.addSeparator();
    menu.addItem (kThemeFolderItem, "Open layouts folder...");
    menu.addItem (kThemeReloadItem, "Reload layouts");

    juce::Component::SafePointer<SolomonEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&layoutButton), [safe] (int id)
    {
        if (safe == nullptr || id == 0) return;
        auto& manager = layouts();
        if (id == kThemeFolderItem)
        {
            manager.folder().createDirectory();
            manager.folder().revealToUser();
        }
        else if (id == kThemeReloadItem)
        {
            manager.refresh();
            safe->say (manager.problems().isEmpty() ? "Layouts reloaded" : "Ignored: " + manager.problems()[0]);
            manager.sendSynchronousChangeMessage();
        }
        else if (id <= (int) manager.layouts().size())
            manager.select (manager.layouts()[(size_t) (id - 1)].name);
    });
}

// ------------------------------------------------------------------------ layout
void SolomonEditor::layoutRow (juce::Rectangle<int> area, std::vector<Knob*> row)
{
    if (row.empty()) return;
    const int w = area.getWidth() / (int) row.size();
    for (auto* k : row)
    {
        auto cell = area.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell.reduced (2));
    }
}

void SolomonEditor::resized()
{
    panels.clear();
    auto area = getLocalBounds().reduced (14);

    // top bar: title | presets | oscillator model | voices + level | volume
    auto top = area.removeFromTop (74);
    auto titleArea = top.removeFromLeft (150);
    title.setBounds (titleArea.removeFromTop (44));
    auto lookRow = titleArea.removeFromTop (24);
    layoutButton.setBounds (lookRow.removeFromRight (52));
    lookRow.removeFromRight (4);
    themeBox.setBounds (lookRow.withTrimmedRight (6));

    placePresetBar (top.removeFromLeft (330).withTrimmedTop (4));
    top.removeFromLeft (12);

    auto vol = top.removeFromRight (90);
    volume->label.setBounds (vol.removeFromTop (14));
    volume->slider.setBounds (vol);
    auto meter = top.removeFromRight (110).withTrimmedTop (22).withTrimmedBottom (14).withTrimmedLeft (8);
    voiceMeter.setBounds (meter);
    auto modelBox = top.removeFromLeft (230).withTrimmedTop (6);
    model->label.setBounds (modelBox.removeFromTop (16));
    model->box.setBounds (modelBox.removeFromTop (30));
    area.removeFromTop (6);

    const int gap = 8;
    const int avail = area.getHeight() - 2 * gap;
    auto row1 = area.removeFromTop (avail * 40 / 100);
    area.removeFromTop (gap);
    auto row2 = area.removeFromTop (avail * 25 / 100);
    area.removeFromTop (gap);
    auto row3 = area;

    // row 1: oscillator | filter (with response curve) | LFO
    {
        const int total = row1.getWidth() - 2 * gap;
        auto osc = row1.removeFromLeft (total * 36 / 100);
        row1.removeFromLeft (gap);
        auto flt = row1.removeFromLeft (total * 42 / 100);
        row1.removeFromLeft (gap);
        auto lfo = row1;
        for (auto r : { osc, flt, lfo }) panels.push_back (r);

        oscHeader.setBounds (osc.removeFromTop (26).reduced (10, 0));
        layoutRow (osc.reduced (6, 0), { harmonics, timbre, morph, tune });

        auto fh = flt.removeFromTop (26).reduced (10, 2);
        filterMode->box.setBounds (fh.removeFromRight (120));
        filterMode->label.setVisible (false);
        filterHeader.setBounds (fh);
        auto fbody = flt.reduced (8, 0);
        filterCurve.setBounds (fbody.removeFromTop (fbody.getHeight() * 45 / 100).reduced (2, 2));
        layoutRow (fbody, { cutoff, resonance });

        auto lh = lfo.removeFromTop (26).reduced (10, 2);
        lfoShape->box.setBounds (lh.removeFromRight (juce::jmin (112, lh.getWidth() - 40)));
        lfoShape->label.setVisible (false);
        lfoHeader.setBounds (lh);
        layoutRow (lfo.reduced (6, 0), { lfoRate });
    }

    // row 2: the two envelopes, knobs on the left and the shape on the right
    {
        auto amp = row2.removeFromLeft ((row2.getWidth() - gap) / 2);
        row2.removeFromLeft (gap);
        auto flt = row2;
        panels.push_back (amp);
        panels.push_back (flt);

        ampHeader.setBounds (amp.removeFromTop (26).reduced (10, 0));
        auto ampBody = amp.reduced (6, 0);
        ampView.setBounds (ampBody.removeFromRight (ampBody.getWidth() * 36 / 100).reduced (6, 6));
        layoutRow (ampBody, { ampA, ampD, ampS, ampR });

        fltEnvHeader.setBounds (flt.removeFromTop (26).reduced (10, 0));
        auto fltBody = flt.reduced (6, 0);
        fltView.setBounds (fltBody.removeFromRight (fltBody.getWidth() * 36 / 100).reduced (6, 6));
        layoutRow (fltBody, { fltA, fltD, fltS, fltR });
    }

    // row 3: modulation matrix (sources down, destinations across)
    {
        panels.push_back (row3);
        matrixHeader.setBounds (row3.removeFromTop (26).reduced (10, 0));
        auto grid = row3.reduced (10, 2);
        auto labelCol = grid.removeFromLeft (110);
        auto head = grid.removeFromTop (18);
        const int cw = grid.getWidth() / kNumModDests;
        for (int d = 0; d < kNumModDests; ++d)
            destLabels[d].setBounds (head.removeFromLeft (cw));
        labelCol.removeFromTop (18);
        const int rh = grid.getHeight() / kNumModSources;
        for (int s = 0; s < kNumModSources; ++s)
        {
            auto cellLabel = labelCol.removeFromTop (rh).reduced (0, 3);
            sourceLabels[s].setBounds (cellLabel.removeFromTop (cellLabel.getHeight() / 2 + 2));
            sourceBars[(size_t) s]->setBounds (cellLabel.reduced (2, 0).withTrimmedRight (6));
            auto rowArea = grid.removeFromTop (rh);
            for (int d = 0; d < kNumModDests; ++d)
            {
                mod[s][d]->label.setBounds ({});
                mod[s][d]->slider.setBounds (rowArea.removeFromLeft (cw).reduced (8, 2));
            }
        }
    }

    applyLayout();
}
