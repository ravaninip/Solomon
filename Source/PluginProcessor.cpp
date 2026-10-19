#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "StateGuard.h"

using namespace solomon;

namespace
{
constexpr const char* kSourceIds[kNumModSources] = { "lfo", "env", "vel", "pres" };
constexpr const char* kDestIds[kNumModDests] = { "timbre", "morph", "harm", "cutoff" };
constexpr float kPitchBendRange = 2.0f;     // semitones

juce::ParameterID id (const juce::String& s) { return { s, 1 }; }

std::unique_ptr<juce::AudioParameterFloat> floatParam (const char* pid, const char* name, juce::NormalisableRange<float> r,
                                                       float def, const char* unit = "")
{
    return std::make_unique<juce::AudioParameterFloat> (id (pid), name, r, def,
                                                        juce::AudioParameterFloatAttributes().withLabel (unit));
}

juce::NormalisableRange<float> range (const Range& r, float skew = 1.0f)
{
    return { r.lo, r.hi, 0.0f, skew };
}
} // namespace

juce::String SolomonProcessor::modId (int source, int dest)
{
    return juce::String ("mod_") + kSourceIds[source] + "_" + kDestIds[dest];
}

juce::AudioProcessorValueTreeState::ParameterLayout SolomonProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray models;
    for (int i = 0; i < kNumModels; ++i) models.add (modelName (i));
    layout.add (std::make_unique<juce::AudioParameterChoice> (id ("model"), "Oscillator", models, 5));

    layout.add (floatParam ("harmonics", "Harmonics", range (ranges::unit), 0.5f));
    layout.add (floatParam ("timbre", "Timbre", range (ranges::unit), 0.5f));
    layout.add (floatParam ("morph", "Morph", range (ranges::unit), 0.5f));
    layout.add (floatParam ("tune", "Tune", range (ranges::tune), 0.0f, "st"));

    layout.add (floatParam ("cutoff", "Cutoff", range (ranges::cutoff, 0.3f), ranges::cutoff.def, "Hz"));
    layout.add (floatParam ("resonance", "Resonance", range (ranges::resonance), ranges::resonance.def));
    layout.add (std::make_unique<juce::AudioParameterChoice> (id ("filterMode"), "Filter Mode",
                                                              juce::StringArray { "Low-pass", "Band-pass", "High-pass" }, 0));

    layout.add (floatParam ("ampA", "Amp Attack", range (ranges::attack, 0.35f), 0.005f, "s"));
    layout.add (floatParam ("ampD", "Amp Decay", range (ranges::decay, 0.35f), 0.4f, "s"));
    layout.add (floatParam ("ampS", "Amp Sustain", range (ranges::sustain), 0.8f));
    layout.add (floatParam ("ampR", "Amp Release", range (ranges::release, 0.35f), 0.25f, "s"));
    layout.add (floatParam ("fltA", "Filter Attack", range (ranges::attack, 0.35f), 0.005f, "s"));
    layout.add (floatParam ("fltD", "Filter Decay", range (ranges::decay, 0.35f), 0.5f, "s"));
    layout.add (floatParam ("fltS", "Filter Sustain", range (ranges::sustain), 0.2f));
    layout.add (floatParam ("fltR", "Filter Release", range (ranges::release, 0.35f), 0.3f, "s"));

    layout.add (floatParam ("lfoRate", "LFO Rate", range (ranges::lfoRate, 0.4f), ranges::lfoRate.def, "Hz"));
    layout.add (std::make_unique<juce::AudioParameterChoice> (id ("lfoShape"), "LFO Shape",
                                                              juce::StringArray { "Sine", "Triangle", "Saw", "Square", "Sample & Hold" }, 0));

    layout.add (floatParam ("volume", "Volume", range (ranges::volume), ranges::volume.def));

    static const char* sourceNames[kNumModSources] = { "LFO", "Env", "Velocity", "Pressure" };
    static const char* destNames[kNumModDests] = { "Timbre", "Morph", "Harmonics", "Cutoff" };
    for (int s = 0; s < kNumModSources; ++s)
        for (int d = 0; d < kNumModDests; ++d)
            layout.add (floatParam (modId (s, d).toRawUTF8(), (juce::String (sourceNames[s]) + " > " + destNames[d]).toRawUTF8(),
                                    range (ranges::modAmount), (s == kModEnv && d == kDestCutoff) ? 0.4f : 0.0f));
    return layout;
}

SolomonProcessor::SolomonProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto get = [this] (const juce::String& pid) { return apvts.getRawParameterValue (pid); };
    p.model = get ("model");           p.harmonics = get ("harmonics");  p.timbre = get ("timbre");
    p.morph = get ("morph");           p.tune = get ("tune");            p.cutoff = get ("cutoff");
    p.resonance = get ("resonance");   p.filterMode = get ("filterMode");
    p.ampA = get ("ampA"); p.ampD = get ("ampD"); p.ampS = get ("ampS"); p.ampR = get ("ampR");
    p.fltA = get ("fltA"); p.fltD = get ("fltD"); p.fltS = get ("fltS"); p.fltR = get ("fltR");
    p.lfoRate = get ("lfoRate");       p.lfoShape = get ("lfoShape");    p.volume = get ("volume");
    for (int s = 0; s < kNumModSources; ++s)
        for (int d = 0; d < kNumModDests; ++d)
            p.mod[s][d] = get (modId (s, d));

    mono.assign (4096, 0.0f);
}

Params SolomonProcessor::readParams() const
{
    Params r;
    r.model = (int) p.model->load();
    r.harmonics = p.harmonics->load(); r.timbre = p.timbre->load(); r.morph = p.morph->load();
    r.tune = p.tune->load();
    r.cutoff = p.cutoff->load(); r.resonance = p.resonance->load(); r.filterMode = (int) p.filterMode->load();
    r.ampA = p.ampA->load(); r.ampD = p.ampD->load(); r.ampS = p.ampS->load(); r.ampR = p.ampR->load();
    r.fltA = p.fltA->load(); r.fltD = p.fltD->load(); r.fltS = p.fltS->load(); r.fltR = p.fltR->load();
    r.lfoRate = p.lfoRate->load(); r.lfoShape = (int) p.lfoShape->load();
    r.volume = p.volume->load();
    for (int s = 0; s < kNumModSources; ++s)
        for (int d = 0; d < kNumModDests; ++d)
            r.mod[s][d] = p.mod[s][d]->load();
    r.sanitize();                      // the engine never sees out-of-range or non-finite values
    return r;
}

bool SolomonProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SolomonProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate);
    if (samplesPerBlock > (int) mono.size())
        mono.assign ((size_t) samplesPerBlock, 0.0f);       // allocated here, never in processBlock
}

void SolomonProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                              synth.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())                        synth.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff();
    else if (m.isPitchWheel())                     synth.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f * kPitchBendRange);
    else if (m.isChannelPressure())                synth.setPressure ((float) m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())                     synth.setPressure ((float) m.getAfterTouchValue() / 127.0f);
    else if (m.isController())
    {
        const float v = (float) m.getControllerValue() / 127.0f;
        if (m.getControllerNumber() == 1)  synth.setModWheel (v);
        if (m.getControllerNumber() == 64) synth.setSustainPedal (m.getControllerValue() >= 64);
    }
}

void SolomonProcessor::renderSegment (int start, int length)
{
    // The scratch buffer is pre-sized; hosts that exceed the announced block size are handled in
    // slices instead of allocating on the audio thread.
    while (length > 0)
    {
        const int n = juce::jmin (length, (int) mono.size());
        synth.render (mono.data(), n);
        for (int ch = 0; ch < currentBuffer->getNumChannels(); ++ch)
            juce::FloatVectorOperations::copy (currentBuffer->getWritePointer (ch, start), mono.data(), n);
        start += n;
        length -= n;
    }
}

void SolomonProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    currentBuffer = &buffer;
    synth.setParams (readParams());

    // sample-accurate MIDI: render up to each event, then apply it
    int pos = 0;
    const int total = buffer.getNumSamples();
    for (const auto metadata : midi)
    {
        const int at = juce::jlimit (pos, total, metadata.samplePosition);
        renderSegment (pos, at - pos);
        pos = at;
        handleMidi (metadata.getMessage());
    }
    renderSegment (pos, total - pos);

    midi.clear();
    currentBuffer = nullptr;
}

juce::AudioProcessorEditor* SolomonProcessor::createEditor()
{
    return new SolomonEditor (*this);
}

void SolomonProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SolomonProcessor::setStateInformation (const void* data, int size)
{
    if (size <= 0 || ! isPlausibleState (data, (size_t) size)) return;
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SolomonProcessor();
}
