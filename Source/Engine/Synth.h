#pragma once
#include <array>
#include <memory>

#include "Envelope.h"
#include "Filter.h"
#include "Lfo.h"
#include "Params.h"
#include "PlaitsOscillator.h"
#include "Resampler.h"

namespace solomon
{
// Four-voice paraphonic synthesiser: every voice has its own oscillator and amplitude envelope,
// while filter, filter envelope and LFO are shared (like the classic paraphonic architecture).
//
// The core runs at a fixed 48 kHz, in blocks of kBlockSize samples, and its output is converted to
// the host sample rate by a band-limited resampler. No JUCE dependency: fully testable offline.
// Not thread-safe: call everything from the audio thread (the plug-in layer does).
class Synth
{
public:
    static constexpr int kNumVoices = 4;

    Synth();

    void prepare (double hostSampleRate);
    void reset();
    void setParams (const Params& p);

    void noteOn (int note, float velocity);            // velocity 0..1
    void noteOff (int note);
    void allNotesOff();
    void setSustainPedal (bool down);
    void setPressure (float value);                     // 0..1 (channel/poly aftertouch)
    void setModWheel (float value);                     // 0..1
    void setPitchBend (float semitones);

    // Renders n mono samples at the host rate.
    void render (float* out, int n);

    int activeVoices() const;

private:
    struct Voice
    {
        std::unique_ptr<PlaitsOscillator> osc = std::make_unique<PlaitsOscillator>();
        Envelope amp;
        int note = -1;
        float velocity = 0.0f;
        bool gate = false;          // key is down
        bool sustained = false;     // key released while the pedal was down
        bool fresh = false;         // first block after note-on: send the trigger edge
        unsigned long long age = 0;
    };

    // Smoothed control value (one-pole, advanced once per core block).
    struct Smooth
    {
        float value = 0.0f;
        void jump (float v) { value = v; }
        float step (float target, float coef) { return value += (target - value) * coef; }
    };

    void renderCore (float* dst, int n);
    int allocateVoice (int note);
    void releaseVoice (Voice& v);

    std::array<Voice, kNumVoices> voices;
    Envelope filterEnv;
    Lfo lfo;
    StateVariableFilter filter;
    Resampler resampler;

    Params params;
    Smooth sHarm, sTimbre, sMorph, sTune, sLogCutoff, sRes, sVolume;
    float blockCoef = 0.05f;

    unsigned long long clock = 0;
    int lastVelocityNote = -1;
    float lastVelocity = 0.0f;
    float pressure = 0.0f, modWheel = 0.0f, bend = 0.0f;
    bool sustainDown = false;
    bool firstBlock = true;
    float prevCutoffHz = 8000.0f, prevRes = 0.15f;
};
} // namespace solomon
