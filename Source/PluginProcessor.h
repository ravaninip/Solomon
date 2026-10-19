#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

#include "Engine/Synth.h"
#include "Presets.h"

class SolomonProcessor : public juce::AudioProcessor
{
public:
    SolomonProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Solomon"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }   // longest release time
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    // Written by the audio thread after every block, read by the editor (relaxed atomics).
    struct Meters
    {
        std::atomic<float> cutoffHz { 8000.0f }, lfo { 0.0f }, env { 0.0f }, velocity { 0.0f }, pressure { 0.0f };
        std::atomic<float> peak { 0.0f };
        std::atomic<int> voiceMask { 0 };
    } meters;

    // Parameter ids of the modulation matrix cells, e.g. "mod_lfo_timbre".
    static juce::String modId (int source, int dest);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    solomon::Params readParams() const;
    void handleMidi (const juce::MidiMessage&);
    void renderSegment (int start, int length);

    solomon::Synth synth;
    std::vector<float> mono;

    struct Cached
    {
        std::atomic<float>* model = nullptr;
        std::atomic<float>* harmonics = nullptr; std::atomic<float>* timbre = nullptr;
        std::atomic<float>* morph = nullptr;     std::atomic<float>* tune = nullptr;
        std::atomic<float>* cutoff = nullptr;    std::atomic<float>* resonance = nullptr;
        std::atomic<float>* filterMode = nullptr;
        std::atomic<float>* ampA = nullptr; std::atomic<float>* ampD = nullptr;
        std::atomic<float>* ampS = nullptr; std::atomic<float>* ampR = nullptr;
        std::atomic<float>* fltA = nullptr; std::atomic<float>* fltD = nullptr;
        std::atomic<float>* fltS = nullptr; std::atomic<float>* fltR = nullptr;
        std::atomic<float>* lfoRate = nullptr; std::atomic<float>* lfoShape = nullptr;
        std::atomic<float>* volume = nullptr;
        std::atomic<float>* mod[solomon::kNumModSources][solomon::kNumModDests] = {};
    } p;

    juce::AudioBuffer<float>* currentBuffer = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SolomonProcessor)
};
