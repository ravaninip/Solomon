#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

// Factory and user presets. A preset is simply a set of parameter values: applying one first resets
// every parameter to its default, then sets the ones the preset lists, so presets never inherit
// leftovers from the previous sound.
class PresetManager
{
public:
    struct Entry
    {
        juce::String name;
        bool factory = true;
        juce::File file;        // user presets only
    };

    PresetManager (juce::AudioProcessorValueTreeState& state, juce::File userFolder);

    // ~/Library/Application Support/Solomon/Presets, %APPDATA%\Solomon\Presets, ~/.config/Solomon/Presets
    static juce::File defaultUserFolder();
    static constexpr const char* kExtension = ".solomonpreset";
    static constexpr int kMaxUserPresets = 500;
    static constexpr int kMaxNameLength = 48;

    void refresh (bool scanUserFolder = true);              // factory list, plus the user folder if asked
    const std::vector<Entry>& entries() const { return list; }
    int numFactory() const { return factoryCount; }

    bool load (int index);
    // Saves the current sound under `name` (existing user presets with that name are replaced).
    bool save (const juce::String& name);
    bool remove (int index);                                // user presets only

    // Checks the factory table against the real parameter set (unknown ids, values out of range).
    // Returns an empty string when everything is consistent.
    juce::String validateFactory() const;

    juce::String currentName() const;                       // name stored in the session, may be empty
    int indexOfCurrent() const;                             // -1 when the sound is not a known preset
    static juce::String cleanName (const juce::String& raw);
    juce::File folder() const { return userDir; }

private:
    void applyFactory (int index);

    juce::AudioProcessorValueTreeState& apvts;
    juce::File userDir;
    std::vector<Entry> list;
    int factoryCount = 0;
};
