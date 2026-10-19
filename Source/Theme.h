#pragma once
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include <vector>

// Colour themes for the editor. A theme is a small JSON file (see docs/THEMES.md) that can be designed
// in the Theme Studio (tools/theme-studio). Files are untrusted input: they are size-limited, strictly
// parsed and checked for readability before they are accepted.
struct Theme
{
    juce::String name { "Midnight" };
    juce::Colour background { 0xff161a20 };    // window
    juce::Colour panel { 0xff1d232b };         // section panels
    juce::Colour well { 0xff12161b };          // recessed areas: graphs, meters, slider tracks
    juce::Colour accent { 0xff3ddc97 };        // headers, knob arcs, live curves
    juce::Colour warm { 0xffffa94d };          // negative modulation, high-level warning
    juce::Colour text { 0xffe6ebf0 };          // values, menu text
    juce::Colour dim { 0xff8b97a5 };           // labels, scales
    juce::Colour edge { 0xff2c343e };          // grid lines, knob tracks
    juce::Colour control { 0xff232a33 };       // menus, drop-downs, tooltips
    juce::Colour controlEdge { 0xff343d49 };   // outline of menus and drop-downs
    juce::Colour danger { 0xffff5d5d };        // clipping
    float panelRadius = 8.0f;                  // corner radius of the panels, in pixels

    static constexpr int kVersion = 1;
    static constexpr int kMaxBytes = 8 * 1024;
    static constexpr int kMaxNameLength = 48;
    static constexpr float kMaxRadius = 24.0f;
    static constexpr const char* kFormat = "plugin-theme";

    // The themes that ship with the plug-in; the first one is the default.
    static const std::vector<Theme>& builtIn();

    // Parses and validates a theme file. Missing colours keep the Midnight values. On failure `error`
    // says why and `out` is left untouched.
    static bool parse (const juce::String& json, Theme& out, juce::String& error);
    juce::String toJson() const;

    // Why this theme would be unreadable ("" when it is fine). Applied to every theme that is loaded, so
    // that a bad theme can never lock a user out of the theme menu.
    juce::String readabilityProblem() const;
    static double contrastRatio (juce::Colour a, juce::Colour b);       // WCAG, 1..21
    static juce::String cleanName (const juce::String& raw);
};

// The list of available themes and the one in use. The choice is a user preference shared by every
// instance of the plug-in: it is kept in a small file next to the user themes. Message thread only.
class ThemeManager : public juce::ChangeBroadcaster
{
public:
    ThemeManager (juce::File themesFolder, juce::File settingsFile);

    // ~/Library/Application Support/Solomon/Themes, %APPDATA%\Solomon\Themes, ~/.config/Solomon/Themes
    static juce::File defaultFolder();
    static ThemeManager& shared();
    static constexpr int kMaxUserThemes = 100;

    void refresh();                                         // re-reads the folder
    const std::vector<Theme>& themes() const { return list; }
    int numBuiltIn() const { return builtInCount; }
    const juce::StringArray& problems() const { return skipped; }   // files ignored by the last refresh

    const Theme& current() const { return list[(size_t) currentIndex]; }
    bool select (const juce::String& name);                 // false if there is no such theme
    int indexOf (const juce::String& name) const;
    juce::File folder() const { return dir; }

private:
    juce::File dir, settings;
    std::vector<Theme> list;
    int builtInCount = 0, currentIndex = 0;
    juce::StringArray skipped;
};
