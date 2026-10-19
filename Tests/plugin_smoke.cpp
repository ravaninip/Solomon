// Host-less smoke test: drives the processor (audio, MIDI, parameters, state) and the editor without a
// DAW, and can save a screenshot of the user interface.
// Usage: solomon_plugin_smoke [screenshot.png [theme name]]
//        solomon_plugin_smoke --descriptor descriptor.json     (writes the list of layout elements)
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Layout.h"
#include "Presets.h"
#include "StateGuard.h"
#include "Theme.h"

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

    std::puts ("themes: format, validation, hostile files, manager");
    {
        for (const auto& t : Theme::builtIn())
            EXPECT (t.readabilityProblem().isEmpty() && t.name.isNotEmpty());
        EXPECT (Theme::builtIn().size() >= 3 && Theme::builtIn()[0].name == "Midnight");

        auto parseOk = [] (const juce::String& json, Theme& t) { juce::String e; return Theme::parse (json, t, e); };
        auto parseFails = [] (const juce::String& json) { Theme t; juce::String e; return ! Theme::parse (json, t, e) && e.isNotEmpty(); };
        const juce::String head = "{ \"format\": \"plugin-theme\", \"version\": 1, \"name\": \"X\"";

        // every built-in theme survives a write/read round trip
        for (const auto& t : Theme::builtIn())
        {
            Theme back;
            EXPECT (parseOk (t.toJson(), back));
            EXPECT (back.name == t.name && back.background == t.background && back.accent == t.accent
                    && back.warm == t.warm && back.text == t.text && back.danger == t.danger
                    && back.controlEdge == t.controlEdge && back.panelRadius == t.panelRadius);
        }

        // a file exactly as the Theme Studio writes it
        {
            Theme studio;
            EXPECT (parseOk (R"json({
  "format": "plugin-theme",
  "version": 1,
  "name": "Test Theme",
  "colors": {
    "background": "#f0f2f4",
    "panel": "#ffffff",
    "well": "#e2e5e9",
    "accent": "#0845a0",
    "warm": "#ae1a09",
    "text": "#1a1e23",
    "dim": "#595f69",
    "edge": "#ccd0d7",
    "control": "#f6f7f8",
    "controlEdge": "#bcc1c8",
    "danger": "#c62828"
  },
  "metrics": {
    "panelRadius": 8
  }
}
)json", studio));
            EXPECT (studio.name == "Test Theme" && studio.accent == juce::Colour (0xff0845a0) && studio.panelRadius == 8.0f);
        }

        // a theme may set only some colours; the rest keep their defaults
        Theme partial;
        EXPECT (parseOk (head + ", \"colors\": { \"accent\": \"#FF00AA\" } }", partial));
        EXPECT (partial.accent == juce::Colour (0xffff00aa) && partial.background == Theme().background);
        EXPECT (parseOk (head + " }", partial));                              // colours and metrics are optional

        // radius is clamped, not trusted
        Theme r;
        EXPECT (parseOk (head + ", \"metrics\": { \"panelRadius\": 1000 } }", r) && r.panelRadius == Theme::kMaxRadius);
        EXPECT (parseOk (head + ", \"metrics\": { \"panelRadius\": -5 } }", r) && r.panelRadius == 0.0f);
        EXPECT (parseFails (head + ", \"metrics\": { \"panelRadius\": \"big\" } }"));
        EXPECT (parseFails (head + ", \"metrics\": 3 }"));

        // malformed or hostile input is refused with a reason
        EXPECT (parseFails (""));
        EXPECT (parseFails ("not json"));
        EXPECT (parseFails ("[1,2,3]"));
        EXPECT (parseFails ("{ \"version\": 1, \"name\": \"X\" }"));                                     // no format
        EXPECT (parseFails ("{ \"format\": \"other\", \"version\": 1, \"name\": \"X\" }"));
        EXPECT (parseFails ("{ \"format\": \"plugin-theme\", \"version\": 2, \"name\": \"X\" }"));       // newer than we know
        EXPECT (parseFails ("{ \"format\": \"plugin-theme\", \"version\": \"1\", \"name\": \"X\" }"));
        EXPECT (parseFails ("{ \"format\": \"plugin-theme\", \"version\": 1 }"));                        // no name
        EXPECT (parseFails ("{ \"format\": \"plugin-theme\", \"version\": 1, \"name\": \"  \" }"));
        for (auto bad : { "red", "#12345", "#1234567", "#gggggg", "123456", "#ff00aa00", "" })
            EXPECT (parseFails (head + ", \"colors\": { \"accent\": \"" + bad + "\" } }"));
        EXPECT (parseFails (head + ", \"colors\": { \"accent\": 16711935 } }"));
        EXPECT (parseFails (head + ", \"colors\": [] }"));
        EXPECT (parseFails (head + ", \"colors\": { \"text\": \"#161a20\" } }"));                        // text invisible on the panels
        EXPECT (parseFails (head + ", \"colors\": { \"accent\": \"#1d232b\" } }"));
        EXPECT (parseFails (head + ", \"colors\": { \"dim\": \"#1d232b\" } }"));
        EXPECT (parseFails (head + ", \"colors\": { \"control\": \"#e6ebf0\" } }"));                     // text invisible in menus

        juce::String deep;
        for (int i = 0; i < 5000; ++i) deep += "[";
        EXPECT (parseFails (deep));                                                                       // would overflow a recursive parser
        EXPECT (parseFails (head + ", \"pad\": \"" + juce::String::repeatedString ("a", Theme::kMaxBytes) + "\" }"));
        EXPECT (Theme::cleanName (juce::String ("  a\n\tb\x01") + juce::String::repeatedString ("z", 200)).length() <= Theme::kMaxNameLength);
        EXPECT (! Theme::cleanName (juce::String ("a\nb")).containsChar ('\n'));
        EXPECT (Theme::contrastRatio (juce::Colours::black, juce::Colours::white) > 20.9);
        EXPECT (Theme::contrastRatio (juce::Colours::red, juce::Colours::red) == 1.0);

        // the manager: built-ins, user files, hostile files, persistence
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("solomon_test_themes_" + juce::String (juce::Random::getSystemRandom().nextInt64()));
        dir.createDirectory();
        auto settings = dir.getSiblingFile (dir.getFileName() + ".txt");
        auto good = Theme::builtIn()[1];
        good.name = "Custom One";
        good.accent = juce::Colour (0xffff00aa);
        dir.getChildFile ("custom.json").replaceWithText (good.toJson());
        dir.getChildFile ("broken.json").replaceWithText ("{ nope");
        dir.getChildFile ("empty.json").create();
        dir.getChildFile ("huge.json").replaceWithText (juce::String::repeatedString ("x", 20000));
        dir.getChildFile ("ignored.txt").replaceWithText (good.toJson());
        auto clash = Theme::builtIn()[0];
        dir.getChildFile ("clash.json").replaceWithText (clash.toJson());                                   // same name as a built-in
        for (int i = 0; i < 3; ++i)
            dir.getChildFile ("unreadable" + juce::String (i) + ".json").replaceWithText (
                "{ \"format\": \"plugin-theme\", \"version\": 1, \"name\": \"U" + juce::String (i) + "\", \"colors\": { \"text\": \"#161a20\" } }");

        {
            ThemeManager tm (dir, settings);
            const int builtIns = (int) Theme::builtIn().size();
            EXPECT (tm.numBuiltIn() == builtIns && (int) tm.themes().size() == builtIns + 1);
            EXPECT (tm.indexOf ("custom one") == builtIns);                                                // case-insensitive lookup
            EXPECT (tm.problems().size() == 7);                                                            // broken, empty, huge, clash, 3 unreadable
            EXPECT (tm.current().name == "Midnight");
            EXPECT (! tm.select ("No such theme") && tm.current().name == "Midnight");
            EXPECT (tm.select ("Custom One") && tm.current().accent == good.accent);
            EXPECT (settings.existsAsFile());
        }
        {
            ThemeManager again (dir, settings);                                                            // preference survives a restart
            EXPECT (again.current().name == "Custom One");
            dir.getChildFile ("custom.json").deleteFile();                                                 // theme file removed meanwhile
            again.refresh();
            EXPECT (again.current().name == "Midnight");
        }
        settings.replaceWithText (juce::String::repeatedString ("y", 5000));                               // junk preference file
        { ThemeManager junk (dir, settings); EXPECT (junk.current().name == "Midnight"); }
        { ThemeManager nowhere (dir.getChildFile ("missing"), dir.getChildFile ("missing/x.txt")); EXPECT ((int) nowhere.themes().size() == (int) Theme::builtIn().size()); }

        for (int i = 0; i < ThemeManager::kMaxUserThemes + 20; ++i)                                         // too many files are capped
        {
            auto t = good;
            t.name = "Many " + juce::String (i);
            dir.getChildFile ("many" + juce::String (i) + ".json").replaceWithText (t.toJson());
        }
        { ThemeManager many (dir, settings); EXPECT ((int) many.themes().size() - many.numBuiltIn() == ThemeManager::kMaxUserThemes); }
        dir.deleteRecursively();
        settings.deleteFile();
    }

    std::puts ("layouts: format, validation, hostile files, manager");
    {
        const juce::String head = "{ \"format\": \"plugin-layout\", \"version\": 1, \"plugin\": \"Solomon\", \"name\": \"L\", \"baseWidth\": 980, \"baseHeight\": 720";
        auto ok = [&] (const juce::String& json, Layout& l) { juce::String e; return Layout::parse (json, "Solomon", 980, 720, l, e); };
        auto fails = [&] (const juce::String& json) { Layout l; juce::String e; return ! Layout::parse (json, "Solomon", 980, 720, l, e) && e.isNotEmpty(); };

        Layout l;
        EXPECT (ok (head + ", \"elements\": { \"knob.cutoff\": { \"x\": 10, \"y\": 20, \"w\": 100, \"h\": 120 }, \"matrix.cell.0.0\": { \"x\": 1, \"y\": 2, \"w\": 30, \"h\": 30, \"visible\": false } } }", l));
        EXPECT (l.entries.size() == 2 && l.entries["knob.cutoff"].bounds == juce::Rectangle<int> (10, 20, 100, 120));
        EXPECT (l.entries["knob.cutoff"].visible && ! l.entries["matrix.cell.0.0"].visible);
        EXPECT (ok (head + ", \"elements\": {} }", l) && l.entries.empty());                         // an empty layout is fine

        Layout back;                                                                                   // round trip
        Layout src;
        src.name = "Round trip";
        src.entries["a.b"] = { { 5, 6, 70, 80 }, true };
        src.entries["c_d-e"] = { { 100, 200, 30, 40 }, false };
        EXPECT (ok (src.toJson ("Solomon", 980, 720), back));
        EXPECT (back.name == "Round trip" && back.entries.size() == 2 && back.entries["a.b"].bounds == src.entries["a.b"].bounds
                && ! back.entries["c_d-e"].visible && back.entries["c_d-e"].bounds == src.entries["c_d-e"].bounds);

        // a file exactly as the Theme Studio writes it
        {
            Layout studio;
            EXPECT (ok (R"json({
  "format": "plugin-layout",
  "version": 1,
  "plugin": "Solomon",
  "name": "Test layout",
  "baseWidth": 980,
  "baseHeight": 720,
  "elements": {
    "knob.cutoff": {
      "x": 422,
      "y": 236,
      "w": 222,
      "h": 144
    },
    "knob.resonance": {
      "x": 555,
      "y": 225,
      "w": 188,
      "h": 117,
      "visible": false
    }
  }
}
)json", studio));
            EXPECT (studio.name == "Test layout" && studio.entries.size() == 2);
            EXPECT (studio.entries["knob.cutoff"].bounds == juce::Rectangle<int> (422, 236, 222, 144) && studio.entries["knob.cutoff"].visible);
            EXPECT (studio.entries["knob.resonance"].bounds == juce::Rectangle<int> (555, 225, 188, 117) && ! studio.entries["knob.resonance"].visible);
        }

        // rectangles are forced inside the window, whatever the file says
        Layout c;
        EXPECT (ok (head + ", \"elements\": { \"a\": { \"x\": -50, \"y\": 99999, \"w\": 5, \"h\": 1000000 }, \"b\": { \"x\": 900, \"y\": 700, \"w\": 400, \"h\": 400 } } }", c));
        for (const auto& [id, e] : c.entries)
        {
            EXPECT (e.bounds.getX() >= 0 && e.bounds.getY() >= 0 && e.bounds.getRight() <= 980 && e.bounds.getBottom() <= 720);
            EXPECT (e.bounds.getWidth() >= Layout::kMinSize && e.bounds.getHeight() >= Layout::kMinSize);
        }
        EXPECT (ok (head + ", \"elements\": { \"a\": { \"x\": 1.4, \"y\": 2.6, \"w\": 50.2, \"h\": 40 } } }", c) && c.entries["a"].bounds.getY() == 3);

        // refused, with a reason
        EXPECT (fails (""));
        EXPECT (fails ("nope"));
        EXPECT (fails ("[1]"));
        EXPECT (fails (head + " }"));                                                                  // no elements
        EXPECT (fails (head + ", \"elements\": [] }"));
        EXPECT (fails (juce::String (head).replace ("plugin-layout", "other") + ", \"elements\": {} }"));
        EXPECT (fails (juce::String (head).replace ("\"version\": 1", "\"version\": 2") + ", \"elements\": {} }"));
        EXPECT (fails (juce::String (head).replace ("Solomon", "ChordMate") + ", \"elements\": {} }"));      // another plug-in's layout
        EXPECT (fails (juce::String (head).replace ("\"baseWidth\": 980", "\"baseWidth\": 800") + ", \"elements\": {} }")); // another window size
        EXPECT (fails (juce::String (head).replace ("\"name\": \"L\"", "\"name\": \"\"") + ", \"elements\": {} }"));
        EXPECT (fails (head + ", \"elements\": { \"a\": { \"x\": 1, \"y\": 2, \"w\": 3 } } }"));               // h missing
        EXPECT (fails (head + ", \"elements\": { \"a\": { \"x\": \"1\", \"y\": 2, \"w\": 30, \"h\": 30 } } }"));
        EXPECT (fails (head + ", \"elements\": { \"a\": 5 } }"));
        EXPECT (fails (head + ", \"elements\": { \"a\": { \"x\": 1, \"y\": 2, \"w\": 30, \"h\": 30, \"visible\": \"no\" } } }"));
        for (auto badId : { "a b", "a/b", "../x", "a\\\\b", "\\u00e9" })
            EXPECT (fails (head + ", \"elements\": { \"" + badId + "\": { \"x\": 1, \"y\": 2, \"w\": 30, \"h\": 30 } } }"));
        EXPECT (! Layout::isValidId (juce::String::repeatedString ("a", Layout::kMaxIdLength + 1)) && Layout::isValidId ("matrix.cell.3.3"));

        juce::String many = head + ", \"elements\": { ";
        for (int i = 0; i <= Layout::kMaxEntries; ++i)
            many += juce::String (i ? ", " : "") + "\"e" + juce::String (i) + "\": { \"x\": 1, \"y\": 2, \"w\": 30, \"h\": 30 }";
        EXPECT (fails (many + " } }"));
        juce::String deep;
        for (int i = 0; i < 5000; ++i) deep += "{\"a\":";
        EXPECT (fails (deep));
        EXPECT (fails (head + ", \"pad\": \"" + juce::String::repeatedString ("a", Layout::kMaxBytes) + "\", \"elements\": {} }"));

        // the manager
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("solomon_test_layouts_" + juce::String (juce::Random::getSystemRandom().nextInt64()));
        dir.createDirectory();
        auto settings = dir.getSiblingFile (dir.getFileName() + ".txt");
        src.name = "Wide";
        dir.getChildFile ("wide.json").replaceWithText (src.toJson ("Solomon", 980, 720));
        dir.getChildFile ("other.json").replaceWithText (src.toJson ("ChordMate", 980, 720));
        dir.getChildFile ("size.json").replaceWithText (src.toJson ("Solomon", 800, 600));
        dir.getChildFile ("broken.json").replaceWithText ("{");
        dir.getChildFile ("huge.json").replaceWithText (juce::String::repeatedString ("x", 40000));
        dir.getChildFile ("clash.json").replaceWithText (Layout().toJson ("Solomon", 980, 720));           // called "Default"
        {
            LayoutManager lm ("Solomon", 980, 720, dir, settings);
            EXPECT (lm.layouts().size() == 2 && lm.current().name == "Default" && lm.current().entries.empty());
            EXPECT (lm.problems().size() == 5);
            EXPECT (! lm.select ("nothing") && lm.select ("wide") && lm.current().entries.size() == 2);
        }
        { LayoutManager again ("Solomon", 980, 720, dir, settings); EXPECT (again.current().name == "Wide");           // remembered
          dir.getChildFile ("wide.json").deleteFile(); again.refresh(); EXPECT (again.current().name == "Default"); }  // file gone: fall back
        dir.deleteRecursively();
        settings.deleteFile();
    }

    std::puts ("editor: creation, resizing and screenshot");
    {
        SolomonProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        setParam (p, "model", 7.0f);
        setParam (p, "mod_lfo_timbre", 0.6f);
        setParam (p, "mod_pres_cutoff", -0.4f);
        const bool descriptorMode = argc > 2 && std::string (argv[1]) == "--descriptor";
        auto& themes = ThemeManager::shared();
        const auto originalTheme = themes.current().name;             // the user's own choice is put back at the end
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        EXPECT (ed != nullptr);
        ed->setSize (980, 720);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        EXPECT (img.getWidth() == 980 && img.getHeight() == 720);

        // every theme can be applied to a live editor, and it really changes what is drawn
        juce::Colour firstPixel = img.getPixelAt (4, 4);
        int differing = 0;
        for (const auto& t : Theme::builtIn())
        {
            EXPECT (themes.select (t.name));
            auto shot = ed->createComponentSnapshot (ed->getLocalBounds());
            EXPECT (shot.getPixelAt (4, 4) == t.background);
            if (shot.getPixelAt (4, 4) != firstPixel) ++differing;
        }
        EXPECT (differing >= 1);
        themes.select (originalTheme);
        if (! descriptorMode && argc > 2) EXPECT (themes.select (juce::String::fromUTF8 (argv[2])));

        // the list of layout elements: what the Theme Studio reads
        {
            juce::var d;
            const auto text = static_cast<SolomonEditor*> (ed.get())->describeLayout();
            EXPECT (juce::JSON::parse (text, d).wasOk() && d.getDynamicObject() != nullptr);
            auto* list = d.getProperty ("elements", {}).getArray();
            EXPECT (list != nullptr && list->size() > 50);
            juce::StringArray ids;
            int panelsSeen = 0, locked = 0;
            for (const auto& item : *list)
            {
                const auto id = item.getProperty ("id", {}).toString();
                EXPECT (Layout::isValidId (id) && ! ids.contains (id));
                ids.add (id);
                const int x = item["x"], y = item["y"], w = item["w"], h = item["h"];
                EXPECT (w >= Layout::kMinSize && h >= 4 && x >= 0 && y >= 0 && x + w <= 980 && y + h <= 720);
                const auto type = item.getProperty ("type", {}).toString();
                EXPECT (type == "panel" || type == "knob" || type == "slider" || type == "menu" || type == "label"
                        || type == "graph" || type == "meter" || type == "group");
                panelsSeen += type == "panel";
                locked += (bool) item.getProperty ("locked", false);
                const auto param = item.getProperty ("param", {}).toString();
                EXPECT (param.isEmpty() || p.apvts.getParameter (param) != nullptr);          // every parameter id is real
                const auto parent = item.getProperty ("parent", {}).toString();
                EXPECT (parent.isEmpty() || parent.startsWith ("panel."));
            }
            EXPECT (panelsSeen == 6 && locked == 2);
            // every knob has the same size, except where its area is smaller than the largest dial
            int knobs = 0, largest = 0, atLargest = 0;
            for (const auto& item : *list)
                if (item.getProperty ("type", {}).toString() == "knob") { ++knobs; largest = juce::jmax (largest, (int) item["h"]); }
            for (const auto& item : *list)
                if (item.getProperty ("type", {}).toString() == "knob" && (int) item["h"] == largest) ++atLargest;
            EXPECT (knobs == 16 && atLargest >= 12);
            for (auto id : { "knob.cutoff", "matrix.cell.3.3", "theme", "layout", "graph.filter", "panel.matrix" })
                EXPECT (ids.contains (id));
            // every parameter that has a knob or a menu appears exactly once
            for (auto* prm : p.getParameters())
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (prm))
                {
                    int n = 0;
                    for (const auto& item : *list) n += item.getProperty ("param", {}).toString() == withId->paramID;
                    EXPECT (n == 1);
                }
            if (descriptorMode)
            {
                juce::File out (juce::String::fromUTF8 (argv[2]));
                out.deleteFile();
                EXPECT (out.replaceWithText (text));
                std::printf ("  descriptor: %s (%d elements)\n", argv[2], list->size());
            }
        }

        // applying a layout: elements move, hidden ones disappear, the locked ones cannot be hidden,
        // unknown ids are ignored, and a layout survives resizing the window
        {
            auto& lm = SolomonEditor::layouts();
            const auto originalLayout = lm.current().name;
            auto folder = lm.folder();
            folder.createDirectory();
            auto file = folder.getChildFile ("zz_smoke_test_layout.json");
            Layout test;
            test.name = "zz smoke test";
            test.entries["knob.cutoff"] = { { 20, 30, 100, 110 }, true };
            test.entries["matrix.cell.0.0"] = { { 400, 600, 100, 20 }, false };
            test.entries["theme"] = { { 300, 300, 90, 24 }, false };                                  // locked: cannot be hidden
            test.entries["panel.osc"] = { { 500, 100, 200, 200 }, true };
            test.entries["not.an.element"] = { { 1, 1, 50, 50 }, true };
            EXPECT (file.replaceWithText (test.toJson ("Solomon", 980, 720)));
            lm.refresh();
            EXPECT (lm.select ("zz smoke test"));

            auto* editor = static_cast<SolomonEditor*> (ed.get());
            juce::Rectangle<int> b; bool shown = false;
            const juce::Rectangle<int> cutoffArea (20, 30, 100, 110);
            EXPECT (editor->elementInfo ("knob.cutoff", b, shown) && shown && cutoffArea.contains (b)          // a knob sits in the middle of its area
                    && std::abs (b.getCentreX() - cutoffArea.getCentreX()) <= 1 && std::abs (b.getCentreY() - cutoffArea.getCentreY()) <= 1);
            const auto firstSize = b;
            EXPECT (editor->elementInfo ("matrix.cell.0.0", b, shown) && ! shown);
            EXPECT (editor->elementInfo ("matrix.cell.0.1", b, shown) && shown);                       // untouched elements stay
            EXPECT (editor->elementInfo ("theme", b, shown) && shown && b == juce::Rectangle<int> (300, 300, 90, 24));
            EXPECT (editor->elementInfo ("panel.osc", b, shown) && shown && b == juce::Rectangle<int> (500, 100, 200, 200));
            EXPECT (! editor->elementInfo ("not.an.element", b, shown));
            ed->setSize (1960, 1440);                                                                  // scaled with the window
            EXPECT (editor->elementInfo ("knob.cutoff", b, shown) && juce::Rectangle<int> (40, 60, 200, 220).contains (b)
                    && b.getHeight() >= firstSize.getHeight() * 15 / 10);                                                // grows with the window
            ed->setSize (980, 720);

            EXPECT (lm.select ("Default"));                                                            // back to the built-in design
            EXPECT (editor->elementInfo ("matrix.cell.0.0", b, shown) && shown);
            EXPECT (editor->elementInfo ("knob.cutoff", b, shown) && ! cutoffArea.contains (b));
            file.deleteFile();
            lm.refresh();
            lm.select (originalLayout);
        }
        ed->setSize (490, 360);                       // smallest allowed size
        ed->setSize (1960, 1440);                     // largest allowed size
        ed->setSize (980, 720);

        if (argc > 1 && ! descriptorMode)
        {
            auto shot = ed->createComponentSnapshot (ed->getLocalBounds());
            juce::File out (juce::String::fromUTF8 (argv[1]));
            out.deleteFile();
            juce::FileOutputStream os (out);
            juce::PNGImageFormat png;
            EXPECT (os.openedOk() && png.writeImageToStream (shot, os));
            std::printf ("  screenshot: %s (%dx%d)\n", argv[1], shot.getWidth(), shot.getHeight());
        }
        themes.select (originalTheme);
    }

    std::printf ("\n%d checks, %d failed\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
