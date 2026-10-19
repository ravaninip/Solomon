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
    setLookAndFeel (nullptr);
}

void SolomonEditor::paint (juce::Graphics& g)
{
    g.fillAll (T().background);
    g.setColour (T().panel);
    for (auto r : panels)
        g.fillRoundedRectangle (r.toFloat(), T().panelRadius);
}

// ------------------------------------------------------------------------ themes
void SolomonEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
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
    themeBox.setBounds (titleArea.removeFromTop (24).withTrimmedRight (8));

    auto presetArea = top.removeFromLeft (330).withTrimmedTop (4);
    {
        auto line = presetArea.removeFromTop (32);
        prevButton.setBounds (line.removeFromLeft (30));
        line.removeFromLeft (4);
        nextButton.setBounds (line.removeFromRight (30));
        line.removeFromRight (4);
        saveButton.setBounds (line.removeFromRight (62));
        line.removeFromRight (4);
        presetBox.setBounds (line);
        presetNote.setBounds (presetArea.removeFromTop (18));
    }
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
}
