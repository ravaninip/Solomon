#pragma once
#include <cmath>
#include <cstdint>

#include "Params.h"

namespace solomon
{
// Control-rate LFO with bipolar output (-1..1).
class Lfo
{
public:
    void reset() { phase = 0.0; held = 0.0f; seed = 0x1234567u; }

    // Advances by `seconds` and returns the new value.
    float advance (float rateHz, int shape, float seconds)
    {
        const double previous = phase;
        phase += (double) rateHz * (double) seconds;
        const bool wrapped = phase >= 1.0;
        phase -= std::floor (phase);

        switch (shape)
        {
            case kLfoTriangle: return (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
            case kLfoSaw:      return (float) (2.0 * phase - 1.0);
            case kLfoSquare:   return phase < 0.5 ? 1.0f : -1.0f;
            case kLfoSampleHold:
                if (wrapped || previous > phase) held = nextRandom();
                return held;
            default:           return (float) std::sin (6.283185307179586 * phase);
        }
    }

private:
    float nextRandom()
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

    double phase = 0.0;
    float held = 0.0f;
    uint32_t seed = 0x1234567u;
};
} // namespace solomon
