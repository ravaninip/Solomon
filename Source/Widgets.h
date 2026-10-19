#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

#include "Engine/Filter.h"

// Small display components used by the editor. They only draw what they are given.
namespace ui
{
inline const juce::Colour kAccent { 0xff3ddc97 };
inline const juce::Colour kWarm { 0xffffa94d };
inline const juce::Colour kText { 0xffe6ebf0 };
inline const juce::Colour kDim { 0xff8b97a5 };
inline const juce::Colour kWell { 0xff12161b };

// Frequency response of the filter on a logarithmic axis: the set curve (dim) and the live curve
// after modulation (bright).
class FilterCurve : public juce::Component, public juce::SettableTooltipClient
{
public:
    struct Settings { float cutoff = 8000.0f, resonance = 0.0f; int mode = 0; };
    std::function<Settings()> settings;

    void setLiveCutoff (float hz) { live = hz; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (kWell);
        g.fillRoundedRectangle (r, 5.0f);
        if (! settings) return;
        const auto s = settings();

        constexpr float lowHz = 20.0f, highHz = 20000.0f, minDb = -42.0f, maxDb = 24.0f;
        auto xOf = [&] (float hz) { return r.getX() + r.getWidth() * std::log (hz / lowHz) / std::log (highHz / lowHz); };
        auto yOf = [&] (float db) { return r.getBottom() - r.getHeight() * (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb); };

        g.setColour (juce::Colour (0xff262d36));
        for (float hz : { 100.0f, 1000.0f, 10000.0f })
            g.drawVerticalLine ((int) xOf (hz), r.getY(), r.getBottom());
        for (float db : { -24.0f, -12.0f, 0.0f, 12.0f })
            g.drawHorizontalLine ((int) yOf (db), r.getX(), r.getRight());
        g.setColour (kDim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("100", (int) xOf (100.0f) + 2, (int) r.getBottom() - 13, 28, 12, juce::Justification::left);
        g.drawText ("1k", (int) xOf (1000.0f) + 2, (int) r.getBottom() - 13, 28, 12, juce::Justification::left);
        g.drawText ("10k", (int) xOf (10000.0f) + 2, (int) r.getBottom() - 13, 28, 12, juce::Justification::left);

        auto curve = [&] (float cutoff)
        {
            juce::Path p;
            const int steps = juce::jmax (8, (int) r.getWidth() / 2);
            for (int i = 0; i <= steps; ++i)
            {
                const float hz = lowHz * std::pow (highHz / lowHz, (float) i / (float) steps);
                const float mag = solomon::StateVariableFilter::magnitudeAt (hz, cutoff, s.resonance, s.mode, 48000.0f);
                const float x = xOf (hz), y = yOf (20.0f * std::log10 (mag + 1.0e-6f));
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        };

        g.setColour (kAccent.withAlpha (0.35f));
        g.strokePath (curve (s.cutoff), juce::PathStrokeType (1.2f));

        auto liveCurve = curve (live > 0.0f ? live : s.cutoff);
        juce::Path fill (liveCurve);
        fill.lineTo (r.getRight(), r.getBottom());
        fill.lineTo (r.getX(), r.getBottom());
        fill.closeSubPath();
        g.setColour (kAccent.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (kAccent);
        g.strokePath (liveCurve, juce::PathStrokeType (2.0f));
    }

private:
    float live = 0.0f;
};

// Shape of an ADSR envelope. Segment widths are compressed (power law) so a 5 ms attack next to a
// 2 s release is still visible.
class EnvelopeView : public juce::Component
{
public:
    struct Adsr { float attack = 0.01f, decay = 0.3f, sustain = 0.7f, release = 0.3f; };
    std::function<Adsr()> adsr;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (kWell);
        g.fillRoundedRectangle (r, 5.0f);
        if (! adsr) return;
        const auto e = adsr();

        auto w = [] (float seconds) { return std::pow (juce::jmax (seconds, 0.001f), 0.35f); };
        const float wa = w (e.attack), wd = w (e.decay), wr = w (e.release);
        const float ws = 0.35f * (wa + wd + wr);
        const float total = wa + wd + ws + wr;

        auto area = r.reduced (6.0f, 8.0f);
        auto X = [&] (float units) { return area.getX() + area.getWidth() * units / total; };
        auto Y = [&] (float level) { return area.getBottom() - area.getHeight() * level; };

        juce::Path p;
        p.startNewSubPath (X (0.0f), Y (0.0f));
        p.lineTo (X (wa), Y (1.0f));
        constexpr int steps = 24;
        for (int i = 1; i <= steps; ++i)                           // decay: exponential towards the sustain level
        {
            const float u = (float) i / steps;
            p.lineTo (X (wa + wd * u), Y (e.sustain + (1.0f - e.sustain) * std::exp (-6.9f * u)));
        }
        p.lineTo (X (wa + wd + ws), Y (e.sustain));
        for (int i = 1; i <= steps; ++i)                           // release: exponential to zero
        {
            const float u = (float) i / steps;
            p.lineTo (X (wa + wd + ws + wr * u), Y (e.sustain * std::exp (-6.9f * u)));
        }

        juce::Path fill (p);
        fill.lineTo (X (total), Y (0.0f));
        fill.lineTo (X (0.0f), Y (0.0f));
        fill.closeSubPath();
        g.setColour (kAccent.withAlpha (0.14f));
        g.fillPath (fill);
        g.setColour (kAccent);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));

        g.setColour (kDim.withAlpha (0.5f));                      // key down / key up markers
        g.drawVerticalLine ((int) X (wa + wd + ws), r.getY() + 4.0f, r.getBottom() - 4.0f);
    }
};

// Four voice lamps and an output level bar with a short peak hold.
class VoiceAndLevel : public juce::Component, public juce::SettableTooltipClient
{
public:
    void set (int voiceMask, float peak)
    {
        mask = voiceMask;
        level = peak > level ? peak : level * 0.88f + peak * 0.12f;      // quick rise, smooth fall
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lamps = r.removeFromTop (14.0f);
        for (int i = 0; i < 4; ++i)
        {
            auto c = lamps.removeFromLeft (lamps.getWidth() / (float) (4 - i)).withSizeKeepingCentre (9.0f, 9.0f);
            g.setColour ((mask >> i) & 1 ? kAccent : juce::Colour (0xff2c343e));
            g.fillEllipse (c);
        }
        r.removeFromTop (4.0f);
        auto bar = r.removeFromTop (8.0f);
        g.setColour (kWell);
        g.fillRoundedRectangle (bar, 3.0f);
        const float db = 20.0f * std::log10 (level + 1.0e-5f);
        const float amount = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        g.setColour (db > -1.0f ? juce::Colour (0xffff5d5d) : db > -9.0f ? kWarm : kAccent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * amount), 3.0f);
    }

private:
    int mask = 0;
    float level = 0.0f;
};

// Live value of a modulation source (bipolar bars grow from the centre).
class SourceBar : public juce::Component
{
public:
    explicit SourceBar (bool bipolarSource) : bipolar (bipolarSource) {}
    void set (float v) { value = v; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth(), 6.0f);
        g.setColour (kWell);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (kAccent);
        if (bipolar)
        {
            const float mid = r.getCentreX(), half = r.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, value);
            g.fillRect (juce::Rectangle<float> (half >= 0 ? mid : mid + half, r.getY(), std::abs (half), r.getHeight()));
            g.setColour (kDim);
            g.drawVerticalLine ((int) mid, r.getY() - 1.0f, r.getBottom() + 1.0f);
        }
        else
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * juce::jlimit (0.0f, 1.0f, value)), 3.0f);
    }

private:
    bool bipolar;
    float value = 0.0f;
};
} // namespace ui
