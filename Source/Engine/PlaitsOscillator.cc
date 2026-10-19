#include "PlaitsOscillator.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "plaits/dsp/engine/additive_engine.h"
#include "plaits/dsp/engine/chord_engine.h"
#include "plaits/dsp/engine/fm_engine.h"
#include "plaits/dsp/engine/grain_engine.h"
#include "plaits/dsp/engine/modal_engine.h"
#include "plaits/dsp/engine/noise_engine.h"
#include "plaits/dsp/engine/particle_engine.h"
#include "plaits/dsp/engine/string_engine.h"
#include "plaits/dsp/engine/swarm_engine.h"
#include "plaits/dsp/engine/virtual_analog_engine.h"
#include "plaits/dsp/engine/waveshaping_engine.h"
#include "plaits/dsp/engine/wavetable_engine.h"
#include "plaits/dsp/engine2/chiptune_engine.h"
#include "plaits/dsp/engine2/phase_distortion_engine.h"
#include "plaits/dsp/engine2/string_machine_engine.h"
#include "plaits/dsp/engine2/virtual_analog_vcf_engine.h"
#include "plaits/dsp/engine2/wave_terrain_engine.h"
#include "stmlib/dsp/limiter.h"

namespace solomon
{
namespace
{
// Names are descriptive on purpose: they do not reuse any third-party model names.
constexpr const char* kModelNames[kNumModels] = {
    "Analog + Filter", "Phase Distortion", "Wave Terrain", "String Machine", "Chiptune",
    "Virtual Analog", "Waveshaper", "FM", "Granular", "Additive", "Wavetable", "Chords",
    "Swarm", "Noise", "Particles", "Plucked String", "Modal Resonator"
};

// Output gain per model, taken from the upstream voice configuration. A negative value means
// "run the output through a limiter with this pre-gain".
constexpr float kOutGain[kNumModels] = {
    1.0f, 0.7f, 0.7f, 0.8f, 0.5f, 0.8f, 0.7f, 0.6f, 0.7f, 0.8f, 0.6f, 0.8f,
    -3.0f, -1.0f, -2.0f, -1.0f, -1.0f
};
} // namespace

namespace
{
constexpr ControlInfo kControls[kNumModels][3] = {
    /* Analog + Filter  */ { { "Response", "Character of the built-in filter" },
                             { "Cutoff", "Cutoff of the built-in filter" },
                             { "Waveform", "Saw, square and pulse, with a sub oscillator" } },
    /* Phase Distortion */ { { "Ratio", "Frequency ratio of the modulating wave" },
                             { "Distortion", "Amount of phase distortion and modulation" },
                             { "Asymmetry", "Pulse width of the modulating wave" } },
    /* Wave Terrain     */ { { "Terrain", "Selects the terrain that is scanned" },
                             { "Radius", "Size of the path scanned across the terrain" },
                             { "Offset", "Shifts the scanned path" } },
    /* String Machine   */ { { "Chord", "Chord played by the divide-down oscillators" },
                             { "Filter", "Filter cutoff and chorus depth" },
                             { "Registration", "Mix of saw and square harmonics, organ style" } },
    /* Chiptune         */ { { "Chord", "Notes of the chord / arpeggio" },
                             { "Arpeggio", "Arpeggio pattern and chord inversion" },
                             { "Pulse", "Pulse width and waveform shape" } },
    /* Virtual Analog   */ { { "Detune", "Interval of the second oscillator" },
                             { "Square", "Shape and sync of the variable square" },
                             { "Saw", "Shape of the variable saw" } },
    /* Waveshaper       */ { { "Shape", "Curve of the waveshaper" },
                             { "Fold", "Wavefolder amount and overtones" },
                             { "Asymmetry", "Asymmetry of the source wave" } },
    /* FM               */ { { "Ratio", "Carrier to modulator frequency ratio" },
                             { "FM Amount", "Modulation index" },
                             { "Feedback", "Feedback of the modulator (centre = none)" } },
    /* Granular         */ { { "Formant Ratio", "Ratio between the two formants" },
                             { "Formant Freq", "Frequency of the formants" },
                             { "Width", "Width and shape of the grains" } },
    /* Additive         */ { { "Bumps", "Number of peaks in the harmonic spectrum" },
                             { "Centroid", "Position of the brightest harmonic" },
                             { "Slope", "How quickly the harmonics fade out" } },
    /* Wavetable        */ { { "Bank", "Wavetable bank" },
                             { "Wave X", "Position along the first table axis" },
                             { "Wave Y", "Position along the second table axis" } },
    /* Chords           */ { { "Chord", "Chord type" },
                             { "Inversion", "Chord inversion and spread" },
                             { "Waveform", "Organ registration morphing into wavetable waves" } },
    /* Swarm            */ { { "Spread", "Random pitch spread of the grains" },
                             { "Density", "Grain rate" },
                             { "Grain Size", "Grain duration and overlap" } },
    /* Noise            */ { { "Filter", "Filter response and spacing of the two filters" },
                             { "Clock", "Clock frequency of the noise source" },
                             { "Resonance", "Filter resonance" } },
    /* Particles        */ { { "Spread", "Random pitch spread of the particles" },
                             { "Density", "How often particles fire" },
                             { "Diffusion", "Below centre: diffusion. Above centre: filter resonance" } },
    /* Plucked String   */ { { "Structure", "String structure and inharmonicity" },
                             { "Brightness", "Brightness of the pluck" },
                             { "Damping", "How fast the string decays" } },
    /* Modal Resonator  */ { { "Structure", "Inharmonic structure of the resonator" },
                             { "Brightness", "Brightness of the strike" },
                             { "Damping", "How fast the resonator decays" } },
};
} // namespace

const ControlInfo& modelControl (int model, int control)
{
    return kControls[std::clamp (model, 0, kNumModels - 1)][std::clamp (control, 0, 2)];
}

const char* modelName (int model)
{
    return kModelNames[std::clamp (model, 0, kNumModels - 1)];
}

struct PlaitsOscillator::Impl
{
    // All engines of a voice share one scratch memory area, exactly like the upstream voice:
    // only one engine is active at a time and Reset() re-initialises it on every model change.
    static constexpr size_t kRamBytes = 64 * 1024;

    std::vector<char> ram = std::vector<char> (kRamBytes);
    stmlib::BufferAllocator allocator { ram.data(), ram.size() };

    plaits::VirtualAnalogVCFEngine vaVcf;
    plaits::PhaseDistortionEngine phaseDistortion;
    plaits::WaveTerrainEngine waveTerrain;
    plaits::StringMachineEngine stringMachine;
    plaits::ChiptuneEngine chiptune;
    plaits::VirtualAnalogEngine virtualAnalog;
    plaits::WaveshapingEngine waveshaping;
    plaits::FMEngine fm;
    plaits::GrainEngine grain;
    plaits::AdditiveEngine additive;
    plaits::WavetableEngine wavetable;
    plaits::ChordEngine chord;
    plaits::SwarmEngine swarm;
    plaits::NoiseEngine noise;
    plaits::ParticleEngine particle;
    plaits::StringEngine string;
    plaits::ModalEngine modal;

    plaits::Engine* engines[kNumModels] = {};
    stmlib::Limiter limiter;
    int active = -1;
    float aux[kBlockSize] = {};

    Impl()
    {
        plaits::Engine* list[kNumModels] = { &vaVcf, &phaseDistortion, &waveTerrain, &stringMachine,
            &chiptune, &virtualAnalog, &waveshaping, &fm, &grain, &additive, &wavetable, &chord,
            &swarm, &noise, &particle, &string, &modal };
        for (int i = 0; i < kNumModels; ++i)
        {
            engines[i] = list[i];
            // Mirrors the upstream registry: the plucked-string and modal models already carry
            // their own decay, so the engine reports them as enveloped.
            engines[i]->post_processing_settings = { kOutGain[i], 1.0f, i == 15 || i == 16 };
            allocator.Free();                 // every engine starts from the same free memory
            engines[i]->Init (&allocator);
        }
        chiptune.set_envelope_shape (plaits::ChiptuneEngine::NO_ENVELOPE);   // our ADSR does the shaping
        limiter.Init();
    }
};

PlaitsOscillator::PlaitsOscillator() : impl (new Impl) {}
PlaitsOscillator::~PlaitsOscillator() = default;

void PlaitsOscillator::render (int model, float note, float harmonics, float timbre, float morph,
                               float accent, Gate gate, float* out, size_t n)
{
    n = std::min<size_t> (n, kBlockSize);
    model = std::clamp (model, 0, kNumModels - 1);
    auto& d = *impl;
    plaits::Engine* e = d.engines[model];

    if (model != d.active)
    {
        e->LoadUserData (nullptr);
        e->Reset();                           // re-initialises the shared scratch memory for this engine
        d.limiter.Init();
        d.active = model;
    }

    auto clamp01 = [] (float v) { return std::isfinite (v) ? std::clamp (v, 0.0f, 1.0f) : 0.5f; };

    plaits::EngineParameters p;
    p.trigger = gate == Gate::Rising ? (plaits::TRIGGER_RISING_EDGE | plaits::TRIGGER_HIGH)
              : gate == Gate::High   ? plaits::TRIGGER_HIGH
                                     : plaits::TRIGGER_LOW;
    // The upstream pitch constant assumes the hardware's real sample rate (47872.34 Hz, see
    // kCorrectedSampleRate in plaits/dsp/dsp.h). Our core runs at exactly 48000 Hz, so without this
    // correction every note would be 4.6 cents sharp.
    static const float kPitchCorrection = (float) (-12.0 * std::log2 (48000.0 / 47872.34));
    p.note = (std::isfinite (note) ? std::clamp (note, -119.0f, 120.0f) : 60.0f) + kPitchCorrection;
    p.harmonics = clamp01 (harmonics);
    p.timbre = clamp01 (timbre);
    p.morph = clamp01 (morph);
    p.accent = clamp01 (accent);

    bool alreadyEnveloped = e->post_processing_settings.already_enveloped;
    e->Render (p, out, d.aux, n, &alreadyEnveloped);

    const float gain = kOutGain[model];
    if (gain < 0.0f)
    {
        d.limiter.Process (-gain, out, n);
        return;
    }
    for (size_t i = 0; i < n; ++i)
        out[i] *= gain;
}
} // namespace solomon
