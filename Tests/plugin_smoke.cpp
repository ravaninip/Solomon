// Host-less smoke test: drives processor and editor without a DAW and saves a UI screenshot.
// Usage: solomon_plugin_smoke [screenshot.png]
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>

static int g_fail = 0, g_checks = 0;
#define EXPECT(...)                                                                       \
    do { ++g_checks; if (! (__VA_ARGS__)) { ++g_fail;                                     \
        std::printf ("  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #__VA_ARGS__); } } while (0)

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    SolomonProcessor p;
    p.setRateAndBufferSizeDetails (44100.0, 512);
    p.prepareToPlay (44100.0, 512);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    p.processBlock (buf, midi);
    EXPECT (std::isfinite (buf.getMagnitude (0, 512)));

    // state round trip through the host API, plus a hostile blob
    p.gain.store (0.3f);
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    SolomonProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    EXPECT (std::abs (q.gain.load() - 0.3f) < 0.01f);
    const char junk[] = "\xff\xfe garbage";
    q.setStateInformation (junk, (int) sizeof (junk));
    EXPECT (std::abs (q.gain.load() - 0.3f) < 0.01f);

    // editor: create, resize, render to an image
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        EXPECT (ed != nullptr);
        ed->setSize (720, 400);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        EXPECT (img.getWidth() == 720 && img.getHeight() == 400);
        if (argc > 1)
        {
            juce::File out (juce::String::fromUTF8 (argv[1]));
            out.deleteFile();
            juce::FileOutputStream os (out);
            juce::PNGImageFormat png;
            EXPECT (os.openedOk() && png.writeImageToStream (img, os));
            std::printf ("  screenshot: %s\n", argv[1]);
        }
    }

    std::printf ("\n%d checks, %d failed\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
