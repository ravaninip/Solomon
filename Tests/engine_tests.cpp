// Engine tests: plain C++, no JUCE, no framework. Run with ctest.
// They cover the pieces that decide how the synth sounds (resampling, filter, envelopes, voices,
// modulation) and the behaviour on hostile or extreme input.
#include "Engine/Envelope.h"
#include "Engine/Filter.h"
#include "Engine/Lfo.h"
#include "Engine/Params.h"
#include "Engine/PlaitsOscillator.h"
#include "Engine/Resampler.h"
#include "Engine/Synth.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

static int g_checks = 0, g_failed = 0;
#define CHECK(...)                                                                     \
    do { ++g_checks; if (! (__VA_ARGS__)) { ++g_failed;                                 \
        std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #__VA_ARGS__); } } while (0)

using namespace solomon;
constexpr double kPi = 3.14159265358979323846;

namespace
{
std::vector<float> renderSynth (Synth& s, int total, int chunk)
{
    std::vector<float> out ((size_t) total);
    for (int done = 0; done < total; done += chunk)
        s.render (out.data() + done, std::min (chunk, total - done));
    return out;
}

bool allFinite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x)) return false;
    return true;
}

float peakOf (const std::vector<float>& v, size_t from = 0)
{
    float p = 0.0f;
    for (size_t i = from; i < v.size(); ++i) p = std::max (p, std::abs (v[i]));
    return p;
}

double rmsOf (const std::vector<float>& v, size_t from = 0)
{
    double s = 0;
    for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / std::max<size_t> (1, v.size() - from));
}

// energy of the first difference: a cheap measure of high-frequency content
double brightness (const std::vector<float>& v, size_t from)
{
    double s = 0;
    for (size_t i = from + 1; i < v.size(); ++i) { const double d = v[i] - v[i - 1]; s += d * d; }
    return s / std::max<size_t> (1, v.size() - from);
}

// amplitude of a sine at `freq` (Hz) inside a signal sampled at `rate` (Goertzel)
double amplitudeAt (const std::vector<float>& v, size_t from, double freq, double rate)
{
    double re = 0, im = 0;
    const size_t n = v.size() - from;
    for (size_t i = 0; i < n; ++i)
    {
        const double a = 2 * kPi * freq * (double) i / rate;
        re += v[from + i] * std::cos (a);
        im += v[from + i] * std::sin (a);
    }
    return 2.0 * std::sqrt (re * re + im * im) / (double) n;
}

Params basicPatch (int model = 5)
{
    Params p;
    p.model = model;
    p.ampA = 0.002f; p.ampS = 1.0f;
    return p;
}
} // namespace

// ------------------------------------------------------------------------------------ params
static void testParams()
{
    Params p;
    p.model = 999; p.harmonics = std::numeric_limits<float>::quiet_NaN(); p.timbre = -5.0f; p.morph = 9.0f;
    p.tune = std::numeric_limits<float>::infinity(); p.cutoff = 1.0e9f; p.resonance = -1.0f;
    p.filterMode = -3; p.lfoShape = 77; p.ampA = 0.0f; p.volume = 100.0f;
    p.mod[0][0] = std::numeric_limits<float>::quiet_NaN(); p.mod[3][3] = 50.0f;
    p.sanitize();
    CHECK (p.model == 16);
    CHECK (p.harmonics == ranges::unit.def);
    CHECK (p.timbre == 0.0f && p.morph == 1.0f);
    CHECK (p.tune == ranges::tune.def);
    CHECK (p.cutoff == ranges::cutoff.hi && p.resonance == 0.0f);
    CHECK (p.filterMode == 0 && p.lfoShape == kNumLfoShapes - 1);
    CHECK (p.ampA == ranges::attack.lo && p.volume == 1.0f);
    CHECK (p.mod[0][0] == 0.0f && p.mod[3][3] == 1.0f);
}

// -------------------------------------------------------------------------------- resampler
static void testResampler()
{
    // equal rates: bit-exact pass-through
    {
        Resampler r;
        r.prepare (48000.0, 48000.0);
        long counter = 0;
        std::vector<float> out (1000);
        r.process (out.data(), 1000, 24, [&] (float* d, int n) { for (int i = 0; i < n; ++i) d[i] = (float) counter++; });
        bool exact = true;
        for (int i = 0; i < 1000; ++i) exact = exact && out[(size_t) i] == (float) i;
        CHECK (exact);
    }

    // in-band sines come out undistorted at every common host rate
    for (double outRate : { 22050.0, 44100.0, 88200.0, 96000.0, 192000.0 })
        for (double f : { 440.0, 5000.0, 15000.0 })
        {
            if (f > 0.4 * outRate) continue;
            Resampler r;
            r.prepare (48000.0, outRate);
            long counter = 0;
            const int n = 6000;
            std::vector<float> out ((size_t) n);
            r.process (out.data(), n, 24, [&] (float* d, int c)
                       { for (int i = 0; i < c; ++i) d[i] = (float) std::sin (2 * kPi * f * (double) counter++ / 48000.0); });
            double maxErr = 0;
            for (int k = 400; k < n; ++k)       // skip the start-up transient (zero history)
                maxErr = std::max (maxErr, std::abs ((double) out[(size_t) k] - std::sin (2 * kPi * f * (double) k / outRate)));
            if (maxErr > 2.0e-3) std::printf ("  resampler %.0f -> %.0f Hz, f=%.0f: max error %.5f\n", 48000.0, outRate, f, maxErr);
            CHECK (maxErr < 2.0e-3);
        }

    // content above the new Nyquist is removed instead of folding back
    {
        Resampler r;
        r.prepare (48000.0, 44100.0);
        long counter = 0;
        const int n = 8000;
        std::vector<float> out ((size_t) n);
        r.process (out.data(), n, 24, [&] (float* d, int c)
                   { for (int i = 0; i < c; ++i) d[i] = (float) std::sin (2 * kPi * 23500.0 * (double) counter++ / 48000.0); });
        const double residual = rmsOf (out, 600) * std::sqrt (2.0);   // amplitude of what is left
        std::printf ("  23.5 kHz tone through 48k->44.1k leaves amplitude %.5f (%.1f dB)\n", residual, 20 * std::log10 (residual + 1e-12));
        CHECK (residual < 0.01);               // better than -40 dB
    }
}

// ------------------------------------------------------------------------------------ filter
static void testFilter()
{
    auto gainAt = [] (float cutoff, float res, int mode, double freq)
    {
        StateVariableFilter f;
        f.setCoefficients (cutoff, res, 48000.0f);
        std::vector<float> out (9600);
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = f.process (0.1f * (float) std::sin (2 * kPi * freq * (double) i / 48000.0), mode);
        return amplitudeAt (out, 2400, freq, 48000.0) / 0.1;
    };

    CHECK (gainAt (1000.0f, 0.0f, kLowPass, 200.0) > 0.9);
    CHECK (gainAt (1000.0f, 0.0f, kLowPass, 8000.0) < 0.03);
    CHECK (gainAt (1000.0f, 0.0f, kHighPass, 8000.0) > 0.9);
    CHECK (gainAt (1000.0f, 0.0f, kHighPass, 200.0) < 0.1);
    CHECK (gainAt (1000.0f, 0.5f, kBandPass, 1000.0) > gainAt (1000.0f, 0.5f, kBandPass, 100.0));
    CHECK (gainAt (1000.0f, 0.9f, kLowPass, 1000.0) > 2.0 * gainAt (1000.0f, 0.0f, kLowPass, 1000.0));   // resonance peak

    // maximum resonance with a loud, fast-moving input stays bounded and finite
    StateVariableFilter f;
    bool ok = true;
    for (int i = 0; i < 200000; ++i)
    {
        f.setCoefficients (20.0f + 19000.0f * (0.5f + 0.5f * (float) std::sin (i * 0.001)), 1.0f, 48000.0f);
        const float y = f.process ((i % 50 < 25) ? 4.0f : -4.0f, i % 3);
        ok = ok && std::isfinite (y) && std::abs (y) < 50.0f;
    }
    CHECK (ok);
}

static void testFilterCurveMatchesFilter()
{
    // The editor draws StateVariableFilter::magnitudeAt, so it has to agree with what the filter does.
    for (int mode = 0; mode < kNumFilterModes; ++mode)
        for (float res : { 0.0f, 0.5f, 0.85f })
            for (float cutoff : { 300.0f, 2000.0f, 9000.0f })
                for (double freq : { 80.0, 400.0, 1500.0, 6000.0, 15000.0 })
                {
                    StateVariableFilter f;
                    f.setCoefficients (cutoff, res, 48000.0f);
                    std::vector<float> out (14400);
                    const float amp = 0.02f;                  // small signal: the soft limiter is transparent
                    for (size_t i = 0; i < out.size(); ++i)
                        out[i] = f.process (amp * (float) std::sin (2 * kPi * freq * (double) i / 48000.0), mode);
                    const double measured = amplitudeAt (out, 4800, freq, 48000.0) / amp;
                    const double predicted = StateVariableFilter::magnitudeAt ((float) freq, cutoff, res, mode, 48000.0f);
                    const double tolerance = 0.03 * std::max (predicted, 1.0) + 0.002;
                    if (std::abs (measured - predicted) > tolerance)
                        std::printf ("  filter curve: mode %d res %.2f fc %.0f f %.0f: measured %.4f predicted %.4f\n",
                                     mode, res, cutoff, freq, measured, predicted);
                    CHECK (std::abs (measured - predicted) <= tolerance);
                }
}

static void testModelInfo()
{
    for (int model = 0; model < kNumModels; ++model)
    {
        CHECK (modelName (model)[0] != '\0');
        for (int c = 0; c < 3; ++c)
        {
            const auto& info = modelControl (model, c);
            CHECK (info.name[0] != '\0' && info.hint[0] != '\0');
            CHECK (std::strlen (info.name) <= 14);          // must fit above a knob
        }
    }
    CHECK (std::strcmp (modelControl (7, 1).name, "FM Amount") == 0);
    CHECK (std::strcmp (modelControl (-5, 99).name, modelControl (0, 2).name) == 0);   // indices are clamped
}

// --------------------------------------------------------------------------- envelope and LFO
static void testEnvelopeAndLfo()
{
    Envelope e;
    e.setSampleRate (48000.0f);
    e.set (0.010f, 0.100f, 0.5f, 0.200f);
    CHECK (! e.active());
    e.gateOn();
    int n = 0;
    while (e.value() < 1.0f && n < 100000) { e.process(); ++n; }
    CHECK (std::abs (n - 480) <= 2);                         // 10 ms attack
    for (int i = 0; i < 48000; ++i) e.process();
    CHECK (std::abs (e.value() - 0.5f) < 1.0e-3f);           // sustain level
    e.gateOff();
    int r = 0;
    while (e.active() && r < 200000) { e.process(); ++r; }
    CHECK (r > 0 && r < 48000 * 0.35);                       // gone shortly after the release time
    CHECK (e.value() == 0.0f);

    Lfo l;
    for (int shape = 0; shape < kNumLfoShapes; ++shape)
    {
        l.reset();
        float lo = 9, hi = -9;
        for (int i = 0; i < 20000; ++i)
        {
            const float v = l.advance (5.0f, shape, 0.0005f);
            lo = std::min (lo, v); hi = std::max (hi, v);
        }
        CHECK (lo >= -1.0001f && hi <= 1.0001f && hi - lo > 0.5f);
    }
}

// ------------------------------------------------------------------------------------ synth
static void testSynthVoices()
{
    Synth s;
    s.prepare (48000.0);
    s.setParams (basicPatch());

    auto silent = renderSynth (s, 4800, 128);
    CHECK (peakOf (silent) == 0.0f);

    s.noteOn (60, 0.8f);
    auto on = renderSynth (s, 24000, 256);
    CHECK (allFinite (on));
    CHECK (rmsOf (on, 12000) > 0.02);
    CHECK (peakOf (on) <= 1.0f);

    s.noteOff (60);
    auto tail = renderSynth (s, 48000, 256);
    CHECK (rmsOf (tail, 40000) < 1.0e-4);
    CHECK (s.activeVoices() == 0);

    // four notes use four voices; a fifth steals the oldest one instead of adding a fifth
    s.noteOn (60, 0.8f); s.noteOn (64, 0.8f); s.noteOn (67, 0.8f); s.noteOn (71, 0.8f);
    renderSynth (s, 2400, 128);
    CHECK (s.activeVoices() == 4);
    s.noteOn (74, 0.8f);
    auto stolen = renderSynth (s, 2400, 128);
    CHECK (s.activeVoices() == 4 && allFinite (stolen));
    s.allNotesOff();
    renderSynth (s, 96000, 512);
    CHECK (s.activeVoices() == 0);

    // sustain pedal keeps released notes sounding until it is lifted
    s.setSustainPedal (true);
    s.noteOn (60, 0.8f);
    renderSynth (s, 4800, 128);
    s.noteOff (60);
    auto held = renderSynth (s, 48000, 256);
    CHECK (rmsOf (held, 24000) > 0.02);
    s.setSustainPedal (false);
    renderSynth (s, 96000, 256);
    CHECK (s.activeVoices() == 0);
}

static void testSynthPitch()
{
    // MIDI note 69 must sound at 440 Hz (and an octave up at 880 Hz) at every host rate, with the
    // tune parameter and pitch bend shifting by the right number of semitones.
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        auto dominant = [&] (int note, float tune, float bend)
        {
            Synth s;
            s.prepare (rate);
            Params p = basicPatch (5);
            p.cutoff = 20000.0f; p.resonance = 0.0f; p.tune = tune;
            p.mod[kModEnv][kDestCutoff] = 0.0f;
            s.setParams (p);
            s.setPitchBend (bend);
            s.noteOn (note, 0.9f);
            const auto out = renderSynth (s, (int) rate, 480);
            // search the fundamental on a fine grid (0.25 Hz) between 100 Hz and 2 kHz
            double best = 0, bestFreq = 0;
            for (double f = 100.0; f < 2000.0; f += 0.25)
            {
                const double a = amplitudeAt (out, (size_t) rate / 4, f, rate);
                if (a > best) { best = a; bestFreq = f; }
            }
            return bestFreq;
        };
        const double a4 = dominant (69, 0.0f, 0.0f);
        const double a5 = dominant (81, 0.0f, 0.0f);
        const double tuned = dominant (69, 12.0f, 0.0f);
        const double bent = dominant (69, 0.0f, 2.0f);
        if (std::abs (a4 - 440.0) > 0.4) std::printf ("  pitch at %.0f Hz: note 69 -> %.2f Hz\n", rate, a4);
        CHECK (std::abs (a4 - 440.0) < 0.4);   // within ~1.5 cents (measurement grid is 0.25 Hz)
        CHECK (std::abs (a5 - 880.0) < 0.8);
        CHECK (std::abs (tuned - 880.0) < 0.8);
        CHECK (std::abs (bent - 440.0 * std::pow (2.0, 2.0 / 12.0)) < 0.6);
    }
}

static void testTelemetry()
{
    Synth s;
    s.prepare (48000.0);
    Params p = basicPatch (5);
    p.mod[kModEnv][kDestCutoff] = 0.0f;
    s.setParams (p);
    renderSynth (s, 2400, 240);
    auto t = s.telemetry();
    CHECK (t.voiceMask == 0 && t.velocity == 0.0f);

    s.noteOn (60, 0.5f);
    s.noteOn (64, 0.5f);
    s.setPressure (0.75f);
    renderSynth (s, 4800, 240);
    t = s.telemetry();
    CHECK (t.voiceMask == 3);                                // two voices sounding
    CHECK (std::abs (t.velocity - 0.5f) < 1.0e-6f);
    CHECK (std::abs (t.pressure - 0.75f) < 1.0e-6f);
    CHECK (t.env > 0.0f && t.env <= 1.0f);
    CHECK (t.lfo >= -1.0f && t.lfo <= 1.0f);
    CHECK (std::abs (t.cutoffHz - p.cutoff) < 1.0f);          // no modulation: reported cut-off = set cut-off

    s.allNotesOff();
    renderSynth (s, 96000, 240);
    CHECK (s.telemetry().voiceMask == 0);
}

static void testSynthBlockSizeIndependence()
{
    // The result must not depend on how the host chops the audio into buffers.
    // (Model 5 has no random component, so two runs are directly comparable.)
    auto run = [] (int chunk)
    {
        Synth s;
        s.prepare (44100.0);
        s.setParams (basicPatch (5));
        s.noteOn (57, 0.7f);
        s.noteOn (64, 0.9f);
        return renderSynth (s, 20000, chunk);
    };
    const auto a = run (512), b = run (1), c = run (77);
    CHECK (a == b);
    CHECK (a == c);
}

static void testSynthAllModels()
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (int model = 0; model < kNumModels; ++model)
        {
            Synth s;
            s.prepare (rate);
            s.setParams (basicPatch (model));
            s.noteOn (48, 0.9f);
            s.noteOn (55, 0.9f);
            const auto out = renderSynth (s, (int) rate / 2, 480);
            const bool finite = allFinite (out);
            const double level = rmsOf (out, out.size() / 2);
            if (! finite || level < 1.0e-4 || peakOf (out) > 1.0f)
                std::printf ("  model %d (%s) at %.0f Hz: finite=%d rms=%.5f peak=%.3f\n", model, modelName (model), rate, finite, level, peakOf (out));
            CHECK (finite);
            CHECK (level > 1.0e-4);          // every model must be audible with default settings
            CHECK (peakOf (out) <= 1.0f);    // and the output stage keeps it bounded
        }
}

static void testSynthExtremes()
{
    // Every model at the corners of its parameter space, with the filter wide open and fully
    // resonant, heavy modulation and a fast LFO: nothing may produce NaN/inf or leave [-1, 1].
    for (int model = 0; model < kNumModels; ++model)
        for (int corner = 0; corner < 8; ++corner)
        {
            Params p = basicPatch (model);
            p.harmonics = (corner & 1) ? 1.0f : 0.0f;
            p.timbre = (corner & 2) ? 1.0f : 0.0f;
            p.morph = (corner & 4) ? 1.0f : 0.0f;
            p.resonance = 1.0f;
            p.cutoff = (corner & 1) ? 20.0f : 20000.0f;
            p.lfoRate = 40.0f;
            p.lfoShape = corner % kNumLfoShapes;
            for (auto& row : p.mod) for (auto& v : row) v = (corner & 2) ? 1.0f : -1.0f;

            Synth s;
            s.prepare (44100.0);
            s.setParams (p);
            s.setPressure (1.0f);
            s.setModWheel (1.0f);
            s.setPitchBend (24.0f);
            for (int note : { 0, 36, 72, 127 }) s.noteOn (note, 1.0f);
            const auto out = renderSynth (s, 22050, 333);
            const bool ok = allFinite (out) && peakOf (out) <= 1.0f;
            if (! ok) std::printf ("  extreme case failed: model %d corner %d\n", model, corner);
            CHECK (ok);
        }
}

static void testSynthModulation()
{
    auto render = [] (float envAmount, float lfoAmount)
    {
        Synth s;
        s.prepare (48000.0);
        Params p = basicPatch (5);
        p.cutoff = 300.0f;
        p.mod[kModEnv][kDestCutoff] = envAmount;
        p.mod[kModLfo][kDestCutoff] = lfoAmount;
        p.lfoRate = 8.0f;
        p.fltA = 0.001f; p.fltD = 4.0f; p.fltS = 1.0f;
        s.setParams (p);
        s.noteOn (48, 0.8f);
        return renderSynth (s, 48000, 240);
    };

    // the filter envelope opens the filter: much brighter than with the matrix cell at zero
    const auto closed = render (0.0f, 0.0f), opened = render (1.0f, 0.0f);
    CHECK (brightness (opened, 24000) > 4.0 * brightness (closed, 24000));

    // the LFO sweeps the filter: brightness varies over time
    const auto swept = render (0.0f, 1.0f);
    double lo = 1e9, hi = 0;
    for (size_t pos = 12000; pos + 2400 <= swept.size(); pos += 2400)
    {
        std::vector<float> win (swept.begin() + (long) pos, swept.begin() + (long) pos + 2400);
        const double b = brightness (win, 0);
        lo = std::min (lo, b); hi = std::max (hi, b);
    }
    CHECK (hi > 2.0 * lo);

    // velocity and pressure reach the matrix as well
    Synth s;
    s.prepare (48000.0);
    Params p = basicPatch (5);
    p.cutoff = 300.0f;
    p.mod[kModEnv][kDestCutoff] = 0.0f;
    p.mod[kModPressure][kDestCutoff] = 1.0f;
    s.setParams (p);
    s.noteOn (48, 0.8f);
    const auto before = renderSynth (s, 24000, 240);
    s.setPressure (1.0f);
    const auto after = renderSynth (s, 24000, 240);
    CHECK (brightness (after, 12000) > 4.0 * brightness (before, 12000));
}

static void testSynthHostileInput()
{
    Synth s;
    s.prepare (0.0);                                   // invalid host rate falls back to the core rate
    Params p;
    p.harmonics = std::numeric_limits<float>::quiet_NaN();
    p.cutoff = std::numeric_limits<float>::infinity();
    s.setParams (p);                                   // sanitised internally
    s.noteOn (-5, 0.5f); s.noteOn (500, 0.5f);         // ignored
    s.noteOn (60, std::numeric_limits<float>::quiet_NaN());
    s.setPressure (std::numeric_limits<float>::infinity());
    s.setPitchBend (std::numeric_limits<float>::quiet_NaN());
    s.noteOff (99);
    const auto out = renderSynth (s, 9600, 100);
    CHECK (allFinite (out) && peakOf (out) <= 1.0f);

    Synth t;
    t.prepare (1.0e9);                                 // absurd host rate must not hang or crash
    t.setParams (Params());
    t.noteOn (60, 0.8f);
    std::vector<float> few (16);
    t.render (few.data(), 16);
    CHECK (allFinite (few));
}

int main()
{
    testParams();
    testResampler();
    testFilter();
    testFilterCurveMatchesFilter();
    testModelInfo();
    testEnvelopeAndLfo();
    testSynthVoices();
    testTelemetry();
    testSynthPitch();
    testSynthBlockSizeIndependence();
    testSynthAllModels();
    testSynthExtremes();
    testSynthModulation();
    testSynthHostileInput();
    std::printf ("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
