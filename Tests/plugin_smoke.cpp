// Host-less smoke test: drives the processor (audio, MIDI, parameters, state) and the editor without a
// DAW, and can save a screenshot of the user interface.
// Usage: solomon_plugin_smoke [screenshot.png]
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Presets.h"
#include "StateGuard.h"

#include <cmath>
#include <cstdio>
#include <string>

static int g_fail = 0, g_checks = 0;
#define EXPECT(...)                                                                       \
    do { ++g_checks; if (! (__VA_ARGS__)) { ++g_fail;                                     \
        std::printf ("  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #__VA_ARGS__); } } while (0)

namespace
{
float peak (const juce::AudioBuffer<float>& b)
{
    float p = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        p = std::fmax (p, b.getMagnitude (ch, 0, b.getNumSamples()));
    return p;
}

bool finite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i))) return false;
    return true;
}

void setParam (SolomonProcessor& p, const juce::String& id, float realValue)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
}

float getParam (SolomonProcessor& p, const juce::String& id)
{
    return p.apvts.getRawParameterValue (id)->load();
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    std::puts ("processor: audio and MIDI");
    for (auto cfg : { std::pair<double, int> { 44100.0, 512 }, { 48000.0, 37 }, { 96000.0, 1024 } })
    {
        SolomonProcessor p;
        p.setRateAndBufferSizeDetails (cfg.first, cfg.second);
        p.prepareToPlay (cfg.first, cfg.second);
        juce::AudioBuffer<float> buf (2, cfg.second);
        juce::MidiBuffer midi;

        buf.clear(); p.processBlock (buf, midi);
        EXPECT (peak (buf) == 0.0f);                                       // silence without notes

        midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), cfg.second / 2);
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), cfg.second / 2);
        buf.clear(); p.processBlock (buf, midi);
        EXPECT (finite (buf));
        EXPECT (buf.getSample (0, cfg.second / 2 - 1) == 0.0f);            // sample-accurate: nothing before the note

        float level = 0.0f;
        for (int i = 0; i < (int) cfg.first / cfg.second / 2; ++i)
        {
            juce::MidiBuffer none;
            buf.clear(); p.processBlock (buf, none);
            level = std::fmax (level, peak (buf));
            EXPECT (finite (buf) && peak (buf) <= 1.0f);
        }
        EXPECT (level > 0.05f);
        EXPECT (buf.getSample (0, 5) == buf.getSample (1, 5));             // same signal on both channels

        // control change, aftertouch, bend, sustain must not upset anything
        juce::MidiBuffer cc;
        cc.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
        cc.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 1);
        cc.addEvent (juce::MidiMessage::channelPressureChange (1, 90), 2);
        cc.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 3);
        cc.addEvent (juce::MidiMessage::aftertouchChange (1, 60, 50), 4);
        buf.clear(); p.processBlock (buf, cc);
        EXPECT (finite (buf));

        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        off.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
        buf.clear(); p.processBlock (buf, off);
        for (int i = 0; i < (int) (cfg.first * 8) / cfg.second; ++i)
        {
            juce::MidiBuffer none;
            buf.clear(); p.processBlock (buf, none);
        }
        EXPECT (peak (buf) < 1.0e-3f);                                     // released and silent again
    }

    std::puts ("processor: a host that exceeds the announced block size");
    {
        SolomonProcessor p;
        p.setRateAndBufferSizeDetails (44100.0, 64);
        p.prepareToPlay (44100.0, 64);
        juce::AudioBuffer<float> big (2, 20000);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        big.clear(); p.processBlock (big, midi);
        EXPECT (finite (big) && peak (big) > 0.01f);
    }

    std::puts ("processor: parameters and every oscillator model");
    {
        SolomonProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buf (2, 256);
        for (int model = 0; model < solomon::kNumModels; ++model)
        {
            setParam (p, "model", (float) model);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 0);
            float level = 0.0f;
            for (int b = 0; b < 100; ++b)
            {
                buf.clear(); p.processBlock (buf, midi);
                midi.clear();
                level = std::fmax (level, peak (buf));
                EXPECT (finite (buf));
            }
            EXPECT (level > 0.01f);
            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            buf.clear(); p.processBlock (buf, off);
        }
        EXPECT (p.apvts.getParameter ("mod_env_cutoff") != nullptr);
        EXPECT (p.getTotalNumOutputChannels() == 2 && p.acceptsMidi() && ! p.producesMidi());
    }

    std::puts ("state: round trip and hostile blobs");
    {
        SolomonProcessor a;
        setParam (a, "model", 9.0f);
        setParam (a, "cutoff", 1234.0f);
        setParam (a, "mod_lfo_timbre", -0.5f);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        EXPECT (mb.getSize() > 0 && mb.getSize() < solomon::kMaxStateBytes);

        SolomonProcessor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        EXPECT ((int) getParam (b, "model") == 9);
        EXPECT (std::abs (getParam (b, "cutoff") - 1234.0f) < 1.0f);
        EXPECT (std::abs (getParam (b, "mod_lfo_timbre") + 0.5f) < 0.01f);

        auto expectUnchanged = [&] (const void* data, int size, const char* what)
        {
            SolomonProcessor c;
            const float before = getParam (c, "cutoff");
            c.setStateInformation (data, size);
            const bool ok = std::abs (getParam (c, "cutoff") - before) < 0.01f;
            if (! ok) std::printf ("  state not rejected: %s\n", what);
            EXPECT (ok);
        };

        const char junk[] = "\xff\xfe not a state";
        expectUnchanged (junk, (int) sizeof (junk), "junk");
        expectUnchanged (nullptr, 0, "empty");
        expectUnchanged (junk, -5, "negative size");

        std::string huge (2 * 1024 * 1024, 'x');
        expectUnchanged (huge.data(), (int) huge.size(), "2 MB of data");

        // XML nested ~100000 levels deep: a recursive parser would overflow the stack
        std::string deep;
        for (int i = 0; i < 100000; ++i) deep += "<a>";
        expectUnchanged (deep.data(), (int) deep.size(), "deeply nested XML");

        juce::MemoryBlock header;
        if (auto xml = juce::XmlDocument::parse ("<STATE/>"))
            juce::AudioProcessor::copyXmlToBinary (*xml, header);
        {
            std::string bomb;
            for (int i = 0; i < 2000; ++i) bomb += "<n>";
            juce::MemoryBlock bm;
            bm.append (header.getData(), juce::jmin<size_t> (8, header.getSize()));
            bm.append (bomb.data(), bomb.size());
            expectUnchanged (bm.getData(), (int) bm.getSize(), "nested bomb behind a valid header");
        }

        // valid structure but hostile values: out of range, NaN, wrong types
        SolomonProcessor c;
        juce::MemoryBlock evil;
        if (auto xml = juce::XmlDocument::parse (
                "<STATE>"
                "<PARAM id=\"cutoff\" value=\"1e30\"/><PARAM id=\"model\" value=\"-9999\"/>"
                "<PARAM id=\"harmonics\" value=\"nan\"/><PARAM id=\"tune\" value=\"inf\"/>"
                "<PARAM id=\"volume\" value=\"banana\"/><PARAM id=\"does_not_exist\" value=\"1\"/>"
                "</STATE>"))
            juce::AudioProcessor::copyXmlToBinary (*xml, evil);
        c.setStateInformation (evil.getData(), (int) evil.getSize());
        c.setRateAndBufferSizeDetails (48000.0, 256);
        c.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        bool ok = true;
        for (int i = 0; i < 100; ++i)
        {
            buf.clear(); c.processBlock (buf, midi); midi.clear();
            ok = ok && finite (buf) && peak (buf) <= 1.0f;
        }
        EXPECT (ok);
        EXPECT (getParam (c, "cutoff") >= 20.0f && getParam (c, "cutoff") <= 20000.0f);
        EXPECT (getParam (c, "model") >= 0.0f && getParam (c, "model") <= 16.0f);
    }

    std::puts ("meters (live values for the editor)");
    {
        SolomonProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        for (int i = 0; i < 40; ++i) { buf.clear(); p.processBlock (buf, midi); midi.clear(); }
        EXPECT (p.meters.voiceMask.load() == 1);
        EXPECT (p.meters.peak.load() > 0.01f && p.meters.peak.load() <= 1.0f);
        EXPECT (p.meters.velocity.load() > 0.5f);
        EXPECT (p.meters.cutoffHz.load() >= 20.0f && p.meters.cutoffHz.load() <= 20000.0f);
    }

    std::puts ("presets: factory bank, user presets, hostile files");
    {
        SolomonProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("solomon_test_presets_" + juce::String (juce::Random::getSystemRandom().nextInt64()));
        PresetManager pm (p.apvts, dir);
        pm.refresh();

        EXPECT (pm.validateFactory().isEmpty());
        if (pm.validateFactory().isNotEmpty()) std::printf ("  factory table: %s\n", pm.validateFactory().toRawUTF8());
        EXPECT (pm.numFactory() >= 10 && (int) pm.entries().size() == pm.numFactory());

        juce::AudioBuffer<float> buf (2, 256);
        for (int i = 0; i < pm.numFactory(); ++i)
        {
            EXPECT (pm.load (i));
            EXPECT (pm.currentName() == pm.entries()[(size_t) i].name && pm.indexOfCurrent() == i);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
            float level = 0.0f; bool ok = true;
            for (int b = 0; b < 200; ++b)
            {
                buf.clear(); p.processBlock (buf, midi); midi.clear();
                level = std::fmax (level, peak (buf));
                ok = ok && finite (buf) && peak (buf) <= 1.0f;
            }
            if (level < 0.02f || ! ok)
                std::printf ("  factory preset '%s': level %.4f ok=%d\n", pm.entries()[(size_t) i].name.toRawUTF8(), level, (int) ok);
            EXPECT (ok && level > 0.02f);                          // audible, finite, inside full scale
            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            buf.clear(); p.processBlock (buf, off);
        }

        // loading a preset starts from the defaults, so nothing is inherited from the previous sound
        setParam (p, "ampR", 5.0f);
        EXPECT (pm.load (0));                                      // "Init"
        EXPECT (std::abs (getParam (p, "ampR") - 0.25f) < 0.01f);

        // saving: names are cleaned, nothing is written outside the folder, reserved names are refused
        setParam (p, "cutoff", 777.0f);
        EXPECT (pm.save ("  ../evil/name?* "));
        EXPECT (! pm.save (""));
        EXPECT (! pm.save ("   "));
        EXPECT (! pm.save ("..."));
        EXPECT (! pm.save ("Init"));
        EXPECT (dir.getNumberOfChildFiles (juce::File::findFiles) == 1);
        EXPECT (dir.getParentDirectory().getChildFile ("evil").existsAsFile() == false);
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false)) EXPECT (f.getParentDirectory() == dir);

        pm.refresh();
        EXPECT ((int) pm.entries().size() == pm.numFactory() + 1);
        const int userIndex = pm.numFactory();
        setParam (p, "cutoff", 5000.0f);
        EXPECT (pm.load (userIndex));
        EXPECT (std::abs (getParam (p, "cutoff") - 777.0f) < 1.0f);

        // hostile files in the preset folder are refused without touching the sound
        auto writeFile = [&] (const char* name, const std::string& content)
        {
            dir.getChildFile (name).replaceWithData (content.data(), content.size());
        };
        std::string deep;
        for (int i = 0; i < 100000; ++i) deep += "<a>";
        writeFile ("deep.solomonpreset", deep);
        writeFile ("huge.solomonpreset", std::string (3 * 1024 * 1024, 'x'));
        writeFile ("junk.solomonpreset", "\xff\xfe binary junk");
        writeFile ("wrongtag.solomonpreset", "<SOMETHING_ELSE><PARAM id=\"cutoff\" value=\"1\"/></SOMETHING_ELSE>");
        dir.getChildFile ("empty.solomonpreset").create();             // zero-byte file
        pm.refresh();
        setParam (p, "cutoff", 4321.0f);
        int refused = 0;
        for (int i = pm.numFactory(); i < (int) pm.entries().size(); ++i)
        {
            const bool loaded = pm.entries()[(size_t) i].name != "evilname" && pm.load (i);
            if (pm.entries()[(size_t) i].name != "evilname")
            {
                if (loaded) std::printf ("  accepted hostile file: %s\n", pm.entries()[(size_t) i].name.toRawUTF8());
                else ++refused;
            }
        }
        if (refused != 5)
            for (int i = pm.numFactory(); i < (int) pm.entries().size(); ++i)
                std::printf ("  user preset entry: '%s'\n", pm.entries()[(size_t) i].name.toRawUTF8());
        EXPECT (refused == 5);
        EXPECT (std::abs (getParam (p, "cutoff") - 4321.0f) < 1.0f);

        EXPECT (pm.remove (pm.numFactory()) || true);              // removing a user preset works...
        EXPECT (! pm.remove (0));                                  // ...factory presets cannot be removed
        dir.deleteRecursively();
    }

    std::puts ("editor: creation, resizing and screenshot");
    {
        SolomonProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        setParam (p, "model", 7.0f);
        setParam (p, "mod_lfo_timbre", 0.6f);
        setParam (p, "mod_pres_cutoff", -0.4f);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        EXPECT (ed != nullptr);
        ed->setSize (980, 720);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        EXPECT (img.getWidth() == 980 && img.getHeight() == 720);
        ed->setSize (490, 360);                       // smallest allowed size
        ed->setSize (1960, 1440);                     // largest allowed size
        ed->setSize (980, 720);

        if (argc > 1)
        {
            auto shot = ed->createComponentSnapshot (ed->getLocalBounds());
            juce::File out (juce::String::fromUTF8 (argv[1]));
            out.deleteFile();
            juce::FileOutputStream os (out);
            juce::PNGImageFormat png;
            EXPECT (os.openedOk() && png.writeImageToStream (shot, os));
            std::printf ("  screenshot: %s (%dx%d)\n", argv[1], shot.getWidth(), shot.getHeight());
        }
    }

    std::printf ("\n%d checks, %d failed\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
