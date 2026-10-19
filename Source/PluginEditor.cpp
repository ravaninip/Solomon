#include "PluginEditor.h"

using namespace solomon;

namespace
{
const juce::Colour kBackground { 0xff161a20 };
const juce::Colour kPanel { 0xff1d232b };
const juce::Colour kAccent { 0xff3ddc97 };
const juce::Colour kText { 0xffe6ebf0 };
const juce::Colour kDim { 0xff8b97a5 };

constexpr int kBaseWidth = 900, kBaseHeight = 620;

void styleHeader (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, kAccent);
    l.setJustificationType (juce::Justification::centredLeft);
}
} // namespace

SolomonEditor::Look::Look()
{
    setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2c343e));
    setColour (juce::Slider::thumbColourId, kText);
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kDim);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff232a33));
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff343d49));
    setColour (juce::ComboBox::arrowColourId, kAccent);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff232a33));
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha (0.35f));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff232a33));
    setColour (juce::TooltipWindow::textColourId, kText);
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

SolomonEditor::SolomonEditor (SolomonProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);

    title.setText ("Solomon", juce::dontSendNotification);
    title.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, kAccent);
    addAndMakeVisible (title);

    model = &addChoice ("model", "Oscillator model", "Sound generator. Harmonics, Timbre and Morph change meaning with the model.");
    filterMode = &addChoice ("filterMode", "Filter", "Filter type");
    lfoShape = &addChoice ("lfoShape", "LFO shape", "Waveform of the LFO");

    harmonics = &addKnob ("harmonics", "Harmonics", "First oscillator control (model dependent)");
    timbre = &addKnob ("timbre", "Timbre", "Second oscillator control (model dependent)");
    morph = &addKnob ("morph", "Morph", "Third oscillator control (model dependent)");
    tune = &addKnob ("tune", "Tune", "Pitch offset in semitones");
    volume = &addKnob ("volume", "Volume", "Output level");
    cutoff = &addKnob ("cutoff", "Cutoff", "Filter cut-off frequency");
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
    styleHeader (lfoHeader, "LFO");            styleHeader (matrixHeader, "MODULATION MATRIX  (aftertouch / mod wheel = Pressure)");
    for (auto* l : { &oscHeader, &filterHeader, &ampHeader, &fltEnvHeader, &lfoHeader, &matrixHeader })
        addAndMakeVisible (l);
    for (int s = 0; s < kNumModSources; ++s)
    {
        sourceLabels[s].setText (srcNames[s], juce::dontSendNotification);
        sourceLabels[s].setJustificationType (juce::Justification::centredRight);
        sourceLabels[s].setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (sourceLabels[s]);
    }
    for (int d = 0; d < kNumModDests; ++d)
    {
        destLabels[d].setText (dstNames[d], juce::dontSendNotification);
        destLabels[d].setJustificationType (juce::Justification::centred);
        destLabels[d].setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (destLabels[d]);
    }

    setResizable (true, true);
    setResizeLimits (kBaseWidth / 2, kBaseHeight / 2, kBaseWidth * 2, kBaseHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) kBaseWidth / kBaseHeight);
    setSize (kBaseWidth, kBaseHeight);
}

SolomonEditor::~SolomonEditor()
{
    setLookAndFeel (nullptr);
}

void SolomonEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);
    g.setColour (kPanel);
    for (auto r : panels)
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
}

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

    // top bar: title, oscillator model, volume
    auto top = area.removeFromTop (74);
    title.setBounds (top.removeFromLeft (190));
    auto vol = top.removeFromRight (100);
    volume->label.setBounds (vol.removeFromTop (14));
    volume->slider.setBounds (vol);
    auto modelBox = top.removeFromLeft (260).withTrimmedLeft (10).withTrimmedTop (6);
    model->label.setBounds (modelBox.removeFromTop (16));
    model->box.setBounds (modelBox.removeFromTop (30));
    area.removeFromTop (6);

    const int gap = 8;
    const int avail = area.getHeight() - 2 * gap;
    auto row1 = area.removeFromTop (avail * 36 / 100);
    area.removeFromTop (gap);
    auto row2 = area.removeFromTop (avail * 29 / 100);
    area.removeFromTop (gap);
    auto row3 = area;

    // row 1: oscillator | filter | LFO
    {
        const int total = row1.getWidth() - 2 * gap;
        auto osc = row1.removeFromLeft (total * 38 / 100);
        row1.removeFromLeft (gap);
        auto flt = row1.removeFromLeft (total * 40 / 100);
        row1.removeFromLeft (gap);
        auto lfo = row1;
        for (auto r : { osc, flt, lfo }) panels.push_back (r);

        oscHeader.setBounds (osc.removeFromTop (26).reduced (10, 0));
        layoutRow (osc.reduced (6, 0), { harmonics, timbre, morph, tune });

        auto fh = flt.removeFromTop (26).reduced (10, 2);
        filterMode->box.setBounds (fh.removeFromRight (120));
        filterMode->label.setVisible (false);
        filterHeader.setBounds (fh);
        layoutRow (flt.reduced (6, 0), { cutoff, resonance });

        auto lh = lfo.removeFromTop (26).reduced (10, 2);
        lfoShape->box.setBounds (lh.removeFromRight (juce::jmin (112, lh.getWidth() - 40)));
        lfoShape->label.setVisible (false);
        lfoHeader.setBounds (lh);
        layoutRow (lfo.reduced (6, 0), { lfoRate });
    }

    // row 2: the two envelopes
    {
        auto amp = row2.removeFromLeft ((row2.getWidth() - gap) / 2);
        row2.removeFromLeft (gap);
        auto flt = row2;
        panels.push_back (amp);
        panels.push_back (flt);
        ampHeader.setBounds (amp.removeFromTop (26).reduced (10, 0));
        layoutRow (amp.reduced (6, 0), { ampA, ampD, ampS, ampR });
        fltEnvHeader.setBounds (flt.removeFromTop (26).reduced (10, 0));
        layoutRow (flt.reduced (6, 0), { fltA, fltD, fltS, fltR });
    }

    // row 3: modulation matrix (sources down, destinations across)
    {
        panels.push_back (row3);
        matrixHeader.setBounds (row3.removeFromTop (26).reduced (10, 0));
        auto grid = row3.reduced (10, 2);
        auto labelCol = grid.removeFromLeft (80);
        auto head = grid.removeFromTop (18);
        const int cw = grid.getWidth() / kNumModDests;
        for (int d = 0; d < kNumModDests; ++d)
            destLabels[d].setBounds (head.removeFromLeft (cw));
        labelCol.removeFromTop (18);
        const int rh = grid.getHeight() / kNumModSources;
        for (int s = 0; s < kNumModSources; ++s)
        {
            sourceLabels[s].setBounds (labelCol.removeFromTop (rh));
            auto rowArea = grid.removeFromTop (rh);
            for (int d = 0; d < kNumModDests; ++d)
            {
                mod[s][d]->label.setBounds ({});
                mod[s][d]->slider.setBounds (rowArea.removeFromLeft (cw).reduced (8, 2));
            }
        }
    }
}
