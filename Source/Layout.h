#pragma once
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include <map>
#include <vector>

// User layouts: where the interface elements sit. A layout is a small JSON file (see docs/LAYOUTS.md) made
// with the Theme Studio. It only lists the elements the user moved, resized or hid; every other element
// stays where the built-in design puts it, so an old layout keeps working when a new version adds controls.
// Files are untrusted input: size-limited, strictly parsed, and every rectangle is forced inside the window.
//
// Nothing here is specific to one plug-in: the plug-in name and the base window size are parameters.
struct Layout
{
    struct Entry
    {
        juce::Rectangle<int> bounds;       // in base-window pixels (the size the design was made at)
        bool visible = true;
    };

    juce::String name { "Default" };
    std::map<juce::String, Entry> entries;       // by element id

    static constexpr int kVersion = 1;
    static constexpr int kMaxBytes = 32 * 1024;
    static constexpr int kMaxEntries = 256;
    static constexpr int kMaxIdLength = 64;
    static constexpr int kMaxNameLength = 48;
    static constexpr int kMinSize = 12;          // smallest width and height of an element
    static constexpr const char* kFormat = "plugin-layout";

    static bool isValidId (const juce::String& id);          // letters, digits, '.', '_', '-'
    static juce::String cleanName (const juce::String& raw);

    // Parses and validates a layout file made for `pluginName` at `baseWidth` x `baseHeight`. Rectangles
    // are clamped into the window. On failure `error` says why and `out` is left untouched.
    static bool parse (const juce::String& json, const juce::String& pluginName, int baseWidth, int baseHeight,
                       Layout& out, juce::String& error);
    juce::String toJson (const juce::String& pluginName, int baseWidth, int baseHeight) const;
};

// The available layouts and the one in use ("Default" = nothing overridden). Like the theme, the choice is a
// preference shared by every instance, kept in a one-line file. Message thread only.
class LayoutManager : public juce::ChangeBroadcaster
{
public:
    LayoutManager (juce::String pluginName, int baseWidth, int baseHeight, juce::File layoutsFolder, juce::File settingsFile);

    static constexpr int kMaxUserLayouts = 100;

    void refresh();                                          // re-reads the folder
    const std::vector<Layout>& layouts() const { return list; }
    const juce::StringArray& problems() const { return skipped; }

    const Layout& current() const { return list[(size_t) currentIndex]; }
    bool select (const juce::String& name);
    int indexOf (const juce::String& name) const;
    juce::File folder() const { return dir; }

private:
    juce::String plugin;
    int baseW, baseH;
    juce::File dir, settings;
    std::vector<Layout> list;
    int currentIndex = 0;
    juce::StringArray skipped;
};
