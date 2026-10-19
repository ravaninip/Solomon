#include "Synth.h"

#include <algorithm>
#include <cmath>

namespace solomon
{
namespace
{
constexpr float kCoreRateF = (float) kCoreRate;
constexpr float kBlockSeconds = (float) kBlockSize / kCoreRateF;
constexpr float kModOctaves = 4.0f;           // cut-off range of a full-scale modulation
constexpr float kVoiceMix = 0.6f;

float clamp01 (float v) { return std::clamp (v, 0.0f, 1.0f); }
} // namespace

Synth::Synth()
{
    for (auto& v : voices) v.amp.setSampleRate (kCoreRateF);
    filterEnv.setSampleRate (kCoreRateF);
    blockCoef = 1.0f - std::exp (-kBlockSeconds / 0.010f);       // ~10 ms smoothing
    setParams (Params());
    prepare (kCoreRate);
}

void Synth::prepare (double hostSampleRate)
{
    // Guard against nonsense from the host: a very low rate would need an enormous kernel.
    const double rate = (std::isfinite (hostSampleRate) && hostSampleRate > 0.0)
                            ? std::clamp (hostSampleRate, 8000.0, 768000.0) : kCoreRate;
    resampler.prepare (kCoreRate, rate);
    reset();
}

void Synth::reset()
{
    for (auto& v : voices)
    {
        v.amp.reset();
        v.note = -1;
        v.gate = v.sustained = v.fresh = false;
    }
    filterEnv.reset();
    lfo.reset();
    filter.reset();
    resampler.reset();
    pressure = modWheel = bend = 0.0f;
    sustainDown = false;
    firstBlock = true;
}

void Synth::setParams (const Params& p)
{
    params = p;
    params.sanitize();
    for (auto& v : voices)
        v.amp.set (params.ampA, params.ampD, params.ampS, params.ampR);
    filterEnv.set (params.fltA, params.fltD, params.fltS, params.fltR);
}

int Synth::allocateVoice (int note)
{
    // 1. the voice already playing this note, 2. a free voice, 3. the oldest releasing voice, 4. the oldest
    for (int i = 0; i < kNumVoices; ++i)
        if (voices[(size_t) i].note == note && voices[(size_t) i].amp.active()) return i;
    for (int i = 0; i < kNumVoices; ++i)
        if (! voices[(size_t) i].amp.active()) return i;

    int best = -1;
    for (int pass = 0; pass < 2 && best < 0; ++pass)
        for (int i = 0; i < kNumVoices; ++i)
        {
            const auto& v = voices[(size_t) i];
            if (pass == 0 && v.gate) continue;                   // first look only at released voices
            if (best < 0 || v.age < voices[(size_t) best].age) best = i;
        }
    return best;
}

void Synth::noteOn (int note, float velocity)
{
    if (note < 0 || note > 127) return;
    velocity = std::isfinite (velocity) ? clamp01 (velocity) : 0.8f;
    auto& v = voices[(size_t) allocateVoice (note)];
    v.note = note;
    v.velocity = velocity;
    v.gate = true;
    v.sustained = false;
    v.fresh = true;
    v.age = ++clock;
    v.amp.gateOn();
    filterEnv.gateOn();
    lastVelocity = velocity;
    lastVelocityNote = note;
}

void Synth::releaseVoice (Voice& v)
{
    v.gate = false;
    v.sustained = false;
    v.amp.gateOff();
}

void Synth::noteOff (int note)
{
    bool anyHeld = false;
    for (auto& v : voices)
        if (v.gate && v.note == note)
        {
            if (sustainDown) v.sustained = true; else releaseVoice (v);
        }
    for (auto& v : voices) anyHeld = anyHeld || v.gate;
    if (! anyHeld && ! sustainDown) filterEnv.gateOff();
}

void Synth::allNotesOff()
{
    for (auto& v : voices) releaseVoice (v);
    filterEnv.gateOff();
}

void Synth::setSustainPedal (bool down)
{
    sustainDown = down;
    if (down) return;
    bool anyHeld = false;
    for (auto& v : voices)
    {
        if (v.sustained) releaseVoice (v);
        anyHeld = anyHeld || v.gate;
    }
    if (! anyHeld) filterEnv.gateOff();
}

void Synth::setPressure (float v)  { pressure = std::isfinite (v) ? clamp01 (v) : 0.0f; }
void Synth::setModWheel (float v)  { modWheel = std::isfinite (v) ? clamp01 (v) : 0.0f; }
void Synth::setPitchBend (float s) { bend = std::isfinite (s) ? std::clamp (s, -24.0f, 24.0f) : 0.0f; }

int Synth::activeVoices() const
{
    int n = 0;
    for (const auto& v : voices) n += v.amp.active() ? 1 : 0;
    return n;
}

void Synth::renderCore (float* dst, int n)
{
    // ---- control rate: smoothing, modulation sources and matrix
    const float targetLogCutoff = std::log2 (params.cutoff);
    if (firstBlock)
    {
        sHarm.jump (params.harmonics); sTimbre.jump (params.timbre); sMorph.jump (params.morph);
        sTune.jump (params.tune); sLogCutoff.jump (targetLogCutoff); sRes.jump (params.resonance);
        sVolume.jump (params.volume);
        firstBlock = false;
    }
    const float harmonics = sHarm.step (params.harmonics, blockCoef);
    const float timbre = sTimbre.step (params.timbre, blockCoef);
    const float morph = sMorph.step (params.morph, blockCoef);
    const float tune = sTune.step (params.tune, blockCoef);
    const float logCutoff = sLogCutoff.step (targetLogCutoff, blockCoef);
    const float resonance = sRes.step (params.resonance, blockCoef);
    const float volume = sVolume.step (params.volume, blockCoef);

    const float lfoValue = lfo.advance (params.lfoRate, params.lfoShape, kBlockSeconds);
    float envValue = 0.0f;
    for (int i = 0; i < n; ++i) envValue = filterEnv.process();

    const float sources[kNumModSources] = {
        lfoValue, envValue, lastVelocity, std::max (pressure, modWheel)
    };
    float mod[kNumModDests] = {};
    for (int s = 0; s < kNumModSources; ++s)
        for (int d = 0; d < kNumModDests; ++d)
            mod[d] += params.mod[s][d] * sources[s];

    const float timbreM = clamp01 (timbre + mod[kDestTimbre]);
    const float morphM = clamp01 (morph + mod[kDestMorph]);
    const float harmonicsM = clamp01 (harmonics + mod[kDestHarmonics]);
    const float cutoffHz = std::clamp (std::exp2 (logCutoff + kModOctaves * mod[kDestCutoff]), 20.0f, 20000.0f);

    // ---- audio rate: voices
    float mix[kBlockSize] = {};
    float osc[kBlockSize];
    for (auto& v : voices)
    {
        if (! v.amp.active()) continue;

        const auto gate = v.fresh ? PlaitsOscillator::Gate::Rising
                        : v.gate  ? PlaitsOscillator::Gate::High
                                  : PlaitsOscillator::Gate::Low;
        v.fresh = false;
        v.osc->render (params.model, (float) v.note + tune + bend, harmonicsM, timbreM, morphM,
                       v.velocity, gate, osc, (size_t) n);
        for (int i = 0; i < n; ++i)
            mix[i] += osc[i] * v.amp.process();
    }

    // ---- shared filter with per-sample coefficient interpolation, then output stage
    const float startCutoff = prevCutoffHz, startRes = prevRes;
    prevCutoffHz = cutoffHz;
    prevRes = resonance;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) (i + 1) / (float) n;
        filter.setCoefficients (startCutoff + (cutoffHz - startCutoff) * t,
                                startRes + (resonance - startRes) * t, kCoreRateF);
        const float y = filter.process (mix[i] * kVoiceMix, params.filterMode) * volume;
        dst[i] = std::tanh (y);
    }
}

void Synth::render (float* out, int n)
{
    resampler.process (out, n, kBlockSize, [this] (float* dst, int count) { renderCore (dst, count); });

    // Band-limited interpolation of a saturated signal can overshoot by ~15 %. Keep the output
    // inside [-1, 1] with a knee that leaves everything below 0.95 untouched.
    constexpr float knee = 0.95f, room = 1.0f - knee;
    for (int i = 0; i < n; ++i)
    {
        const float a = std::abs (out[i]);
        if (a > knee)
            out[i] = std::copysign (knee + room * std::tanh ((a - knee) / room), out[i]);
    }
}
} // namespace solomon
