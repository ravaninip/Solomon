#pragma once
#include <algorithm>
#include <cmath>

namespace solomon
{
enum ModSource { kModLfo, kModEnv, kModVelocity, kModPressure, kNumModSources };
enum ModDest { kDestTimbre, kDestMorph, kDestHarmonics, kDestCutoff, kNumModDests };
enum FilterMode { kLowPass, kBandPass, kHighPass, kNumFilterModes };
enum LfoShape { kLfoSine, kLfoTriangle, kLfoSaw, kLfoSquare, kLfoSampleHold, kNumLfoShapes };

// Value ranges shared by the engine (sanitising) and the plug-in layer (parameter definitions).
struct Range { float lo, hi, def; };

namespace ranges
{
constexpr Range unit { 0.0f, 1.0f, 0.5f };
constexpr Range tune { -24.0f, 24.0f, 0.0f };          // semitones
constexpr Range cutoff { 20.0f, 20000.0f, 8000.0f };   // Hz
constexpr Range resonance { 0.0f, 1.0f, 0.15f };
constexpr Range attack { 0.001f, 4.0f, 0.005f };       // seconds
constexpr Range decay { 0.005f, 4.0f, 0.4f };
constexpr Range sustain { 0.0f, 1.0f, 0.8f };
constexpr Range release { 0.005f, 6.0f, 0.3f };
constexpr Range lfoRate { 0.05f, 40.0f, 2.0f };        // Hz
constexpr Range volume { 0.0f, 1.0f, 0.7f };
constexpr Range modAmount { -1.0f, 1.0f, 0.0f };
} // namespace ranges

// Everything the user can set. The plug-in layer fills it from the host parameters on every block;
// the engine works on a sanitised copy, so out-of-range or non-finite values can never reach the DSP.
struct Params
{
    int model = 5;
    float harmonics = 0.5f, timbre = 0.5f, morph = 0.5f;
    float tune = 0.0f;

    float cutoff = ranges::cutoff.def;
    float resonance = ranges::resonance.def;
    int filterMode = kLowPass;

    float ampA = ranges::attack.def, ampD = ranges::decay.def, ampS = ranges::sustain.def, ampR = 0.25f;
    float fltA = ranges::attack.def, fltD = 0.5f, fltS = 0.2f, fltR = 0.3f;

    float lfoRate = ranges::lfoRate.def;
    int lfoShape = kLfoSine;

    float volume = ranges::volume.def;

    // mod[source][destination], -1..1
    float mod[kNumModSources][kNumModDests] = {};

    Params() { mod[kModEnv][kDestCutoff] = 0.4f; }

    void sanitize();
};

inline float sanitizeValue (float v, const Range& r)
{
    return std::isfinite (v) ? std::clamp (v, r.lo, r.hi) : r.def;
}

inline void Params::sanitize()
{
    model = std::clamp (model, 0, 16);
    harmonics = sanitizeValue (harmonics, ranges::unit);
    timbre = sanitizeValue (timbre, ranges::unit);
    morph = sanitizeValue (morph, ranges::unit);
    tune = sanitizeValue (tune, ranges::tune);
    cutoff = sanitizeValue (cutoff, ranges::cutoff);
    resonance = sanitizeValue (resonance, ranges::resonance);
    filterMode = std::clamp (filterMode, 0, kNumFilterModes - 1);
    ampA = sanitizeValue (ampA, ranges::attack);   ampD = sanitizeValue (ampD, ranges::decay);
    ampS = sanitizeValue (ampS, ranges::sustain);  ampR = sanitizeValue (ampR, ranges::release);
    fltA = sanitizeValue (fltA, ranges::attack);   fltD = sanitizeValue (fltD, ranges::decay);
    fltS = sanitizeValue (fltS, ranges::sustain);  fltR = sanitizeValue (fltR, ranges::release);
    lfoRate = sanitizeValue (lfoRate, ranges::lfoRate);
    lfoShape = std::clamp (lfoShape, 0, kNumLfoShapes - 1);
    volume = sanitizeValue (volume, ranges::volume);
    for (auto& row : mod)
        for (auto& v : row)
            v = sanitizeValue (v, ranges::modAmount);
}
} // namespace solomon
