#pragma once
#include <algorithm>
#include <cmath>

namespace solomon
{
// ADSR envelope. Attack is linear, decay and release are exponential (they reach about -60 dB
// after the nominal time), which sounds natural for both amplitude and filter use.
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (float sr) { sampleRate = sr; }

    void set (float attack, float decay, float sustain, float release)
    {
        attackRate = 1.0f / (std::max (attack, 0.0005f) * sampleRate);
        decayCoef = coefFor (decay);
        releaseCoef = coefFor (release);
        sustainLevel = std::clamp (sustain, 0.0f, 1.0f);
    }

    void gateOn()  { stage = Stage::Attack; }
    void gateOff() { if (stage != Stage::Idle) stage = Stage::Release; }
    void reset()   { stage = Stage::Idle; level = 0.0f; }

    bool active() const { return stage != Stage::Idle; }
    float value() const { return level; }

    float process()
    {
        switch (stage)
        {
            case Stage::Idle: break;
            case Stage::Attack:
                level += attackRate;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                level = sustainLevel + (level - sustainLevel) * decayCoef;
                if (level - sustainLevel < 1.0e-4f) { level = sustainLevel; stage = Stage::Sustain; }
                break;
            case Stage::Sustain:
                level = sustainLevel;          // follows live changes of the sustain level
                break;
            case Stage::Release:
                level *= releaseCoef;
                if (level < 1.0e-4f) { level = 0.0f; stage = Stage::Idle; }
                break;
        }
        return level;
    }

private:
    float coefFor (float seconds) const
    {
        return std::exp (-6.9f / (std::max (seconds, 0.001f) * sampleRate));   // -60 dB in `seconds`
    }

    float sampleRate = 48000.0f;
    float attackRate = 0.01f, decayCoef = 0.999f, releaseCoef = 0.999f, sustainLevel = 0.8f;
    float level = 0.0f;
    Stage stage = Stage::Idle;
};
} // namespace solomon
