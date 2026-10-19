#pragma once
#include <algorithm>
#include <cmath>

#include "Params.h"

namespace solomon
{
// Zero-delay-feedback state-variable filter (topology-preserving transform), low/band/high-pass.
// Stable under fast cutoff modulation; a soft saturator on the input and on the integrator state
// gives the resonance a controlled, slightly gritty character instead of blowing up.
class StateVariableFilter
{
public:
    void reset() { ic1 = ic2 = 0.0f; }

    // cutoff in Hz, resonance 0..1 (1 = near self-oscillation), sampleRate in Hz
    void setCoefficients (float cutoffHz, float resonance, float sampleRate)
    {
        const float fc = std::clamp (cutoffHz, 10.0f, 0.45f * sampleRate);
        g = std::tan (3.14159265f * fc / sampleRate);
        k = 2.0f * (1.0f - 0.96f * std::clamp (resonance, 0.0f, 1.0f));
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float process (float x, int mode)
    {
        x = saturate (x);
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = saturate (2.0f * v1 - ic1);
        ic2 = saturate (2.0f * v2 - ic2);
        switch (mode)
        {
            case kBandPass: return v1;
            case kHighPass: return x - k * v1 - v2;
            default:        return v2;
        }
    }

private:
    // Soft limiter: transparent at normal levels (under 1% error up to |x| = 0.5) and bounded by 4,
    // so a strongly resonant filter compresses its peak instead of blowing up. It must stay this
    // gentle: the states are updated every sample, and a stronger curve would act as a leaky
    // integrator and make the output level depend on the cut-off.
    static float saturate (float x) { return x / std::sqrt (1.0f + 0.0625f * x * x); }

    float g = 0.1f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};
} // namespace solomon
