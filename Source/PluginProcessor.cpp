#include "PluginProcessor.h"
#include "PluginEditor.h"

SolomonProcessor::SolomonProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool SolomonProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SolomonProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;       // some hosts report 0 during scanning
}

void SolomonProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    // TODO: real-time work here. Rules: no allocation, no locks (try-lock at most), no file I/O, no logging.
    buffer.applyGain (gain.load());
    midi.clear();
}

juce::AudioProcessorEditor* SolomonProcessor::createEditor()
{
    return new SolomonEditor (*this);
}

void SolomonProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    state.gain = (double) gain.load();
    const auto s = state.serialize();
    dest.append (s.data(), s.size());
}

void SolomonProcessor::setStateInformation (const void* data, int size)
{
    if (data == nullptr || size <= 0 || (size_t) size > engine::kMaxStateBytes) return;
    if (state.deserialize (std::string ((const char*) data, (size_t) size)))
    {
        gain.store ((float) state.gain);
        stateRevision.fetch_add (1);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SolomonProcessor();
}
