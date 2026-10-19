#pragma once
#include <cstddef>
#include <memory>

namespace solomon
{
// Number of selectable oscillator models.
constexpr int kNumModels = 17;

// Display name of a model (index clamped to the valid range).
const char* modelName (int model);

// What the three macro controls (0 = Harmonics, 1 = Timbre, 2 = Morph) do in each model. The names and
// hints were derived from reading the model source code; they are shown in the editor.
struct ControlInfo
{
    const char* name;
    const char* hint;
};
const ControlInfo& modelControl (int model, int control);

// Samples the oscillator renders per call at most. The synth core always runs at kCoreRate.
constexpr int kBlockSize = 24;
constexpr double kCoreRate = 48000.0;

// One monophonic oscillator voice. It wraps the macro-oscillator engines of the open-source
// Plaits code base (MIT, see third_party/mutable) and hides them behind a pimpl so that the
// third-party headers (and the TEST macro they need) never leak into the rest of the code.
class PlaitsOscillator
{
public:
    PlaitsOscillator();
    ~PlaitsOscillator();
    PlaitsOscillator (const PlaitsOscillator&) = delete;
    PlaitsOscillator& operator= (const PlaitsOscillator&) = delete;

    // Gate state of the voice for the block being rendered.
    enum class Gate { Low, Rising, High };

    // note: MIDI note number (fractional). harmonics/timbre/morph/accent: 0..1.
    // Renders n <= kBlockSize samples at kCoreRate into out.
    void render (int model, float note, float harmonics, float timbre, float morph, float accent,
                 Gate gate, float* out, size_t n);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace solomon
