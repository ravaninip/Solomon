#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
struct ColourField { const char* key; juce::Colour Theme::*member; };

const ColourField kFields[] = {
    { "background", &Theme::background }, { "panel", &Theme::panel }, { "well", &Theme::well },
    { "accent", &Theme::accent },         { "warm", &Theme::warm },   { "text", &Theme::text },
    { "dim", &Theme::dim },               { "edge", &Theme::edge },   { "control", &Theme::control },
    { "controlEdge", &Theme::controlEdge }, { "danger", &Theme::danger },
};

// "#RRGGBB" only: themes are opaque, which also keeps the readability check meaningful.
bool parseColour (const juce::String& s, juce::Colour& out)
{
    if (s.length() != 7 || s[0] != '#') return false;
    for (int i = 1; i < 7; ++i)
        if (juce::CharacterFunctions::getHexDigitValue (s[i]) < 0) return false;
    out = juce::Colour (0xff000000u | (juce::uint32) s.substring (1).getHexValue32());
    return true;
}

juce::String colourText (juce::Colour c) { return "#" + c.toDisplayString (false).toLowerCase(); }

double luminance (juce::Colour c)
{
    auto lin = [] (float v) { return v <= 0.03928f ? (double) v / 12.92 : std::pow (((double) v + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin (c.getFloatRed()) + 0.7152 * lin (c.getFloatGreen()) + 0.0722 * lin (c.getFloatBlue());
}

int bracketCount (const juce::String& s)
{
    int n = 0;
    for (auto c : s) if (c == '{' || c == '[') ++n;
    return n;
}

Theme make (const char* name, uint32_t bg, uint32_t panel, uint32_t well, uint32_t accent, uint32_t warm, uint32_t text,
            uint32_t dim, uint32_t edge, uint32_t control, uint32_t controlEdge, uint32_t danger, float radius)
{
    Theme t;
    t.name = name;
    t.background = juce::Colour (0xff000000u | bg);      t.panel = juce::Colour (0xff000000u | panel);
    t.well = juce::Colour (0xff000000u | well);          t.accent = juce::Colour (0xff000000u | accent);
    t.warm = juce::Colour (0xff000000u | warm);          t.text = juce::Colour (0xff000000u | text);
    t.dim = juce::Colour (0xff000000u | dim);            t.edge = juce::Colour (0xff000000u | edge);
    t.control = juce::Colour (0xff000000u | control);    t.controlEdge = juce::Colour (0xff000000u | controlEdge);
    t.danger = juce::Colour (0xff000000u | danger);      t.panelRadius = radius;
    return t;
}
} // namespace

const std::vector<Theme>& Theme::builtIn()
{
    static const std::vector<Theme> themes = {
        make ("Midnight", 0x161a20, 0x1d232b, 0x12161b, 0x3ddc97, 0xffa94d, 0xe6ebf0, 0x8b97a5, 0x2c343e, 0x232a33, 0x343d49, 0xff5d5d, 8.0f),
        make ("Paper",    0xf3f1ec, 0xffffff, 0xe8e5df, 0x0f8a5f, 0xc26a00, 0x1b1f24, 0x5d6672, 0xd5d1c8, 0xf7f6f2, 0xc9c4b8, 0xc62828, 8.0f),
        make ("Amber",    0x140f08, 0x1d160b, 0x0f0b05, 0xffb000, 0xff6b35, 0xf5e6c8, 0xa08a62, 0x3a2e14, 0x261d0e, 0x4a3a18, 0xff4d4d, 4.0f),
    };
    return themes;
}

double Theme::contrastRatio (juce::Colour a, juce::Colour b)
{
    const double la = luminance (a), lb = luminance (b);
    return (std::max (la, lb) + 0.05) / (std::min (la, lb) + 0.05);
}

juce::String Theme::readabilityProblem() const
{
    struct Rule { const char* what; juce::Colour fg, bg; double need; };
    const Rule rules[] = {
        { "text on background", text, background, 3.0 }, { "text on panel", text, panel, 3.0 },
        { "text on control", text, control, 3.0 },       { "accent on panel", accent, panel, 2.0 },
        { "dim on panel", dim, panel, 1.8 },
    };
    for (const auto& r : rules)
    {
        const double c = contrastRatio (r.fg, r.bg);
        if (c < r.need)
            return juce::String ("hard to read: ") + r.what + " has contrast " + juce::String (c, 1) + ":1, needs at least "
                   + juce::String (r.need, 1) + ":1";
    }
    return {};
}

juce::String Theme::cleanName (const juce::String& raw)
{
    juce::String out;
    for (auto c : raw)
        if (c >= 32 && c != 127) out += c;
    return out.trim().substring (0, kMaxNameLength).trim();
}

bool Theme::parse (const juce::String& json, Theme& out, juce::String& error)
{
    // The JSON parser is recursive: bound the size and the nesting before handing it anything.
    if (json.getNumBytesAsUTF8() > (size_t) kMaxBytes) { error = "file is larger than 8 KiB"; return false; }
    if (bracketCount (json) > 32)                       { error = "too many nested objects"; return false; }

    juce::var root;
    if (juce::JSON::parse (json, root).failed())        { error = "not valid JSON"; return false; }
    auto* obj = root.getDynamicObject();
    if (obj == nullptr)                                 { error = "the top level must be an object"; return false; }

    if (obj->getProperty ("format").toString() != kFormat) { error = "missing \"format\": \"plugin-theme\""; return false; }
    const auto& version = obj->getProperty ("version");
    if (! (version.isInt() || version.isInt64()))       { error = "missing integer \"version\""; return false; }
    if ((int) version != kVersion)                      { error = "unsupported version " + version.toString(); return false; }

    Theme t;
    t.name = cleanName (obj->getProperty ("name").toString());
    if (t.name.isEmpty())                               { error = "missing \"name\""; return false; }

    const auto& colours = obj->getProperty ("colors");
    if (! colours.isVoid())
    {
        auto* c = colours.getDynamicObject();
        if (c == nullptr)                               { error = "\"colors\" must be an object"; return false; }
        for (const auto& f : kFields)
        {
            const auto& v = c->getProperty (f.key);
            if (v.isVoid()) continue;                    // not set: keep the default
            juce::Colour parsed;
            if (! v.isString() || ! parseColour (v.toString(), parsed))
            {
                error = juce::String ("colour \"") + f.key + "\" must look like \"#rrggbb\"";
                return false;
            }
            t.*(f.member) = parsed;
        }
    }

    const auto& metrics = obj->getProperty ("metrics");
    if (! metrics.isVoid())
    {
        auto* m = metrics.getDynamicObject();
        if (m == nullptr)                               { error = "\"metrics\" must be an object"; return false; }
        const auto& r = m->getProperty ("panelRadius");
        if (! r.isVoid())
        {
            if (! (r.isInt() || r.isInt64() || r.isDouble()) || ! std::isfinite ((double) r))
                { error = "\"panelRadius\" must be a number"; return false; }
            t.panelRadius = juce::jlimit (0.0f, kMaxRadius, (float) (double) r);
        }
    }

    if (auto problem = t.readabilityProblem(); problem.isNotEmpty())
        { error = problem; return false; }

    out = t;
    return true;
}

juce::String Theme::toJson() const
{
    auto* colours = new juce::DynamicObject();
    for (const auto& f : kFields)
        colours->setProperty (f.key, colourText (this->*(f.member)));
    auto* metrics = new juce::DynamicObject();
    metrics->setProperty ("panelRadius", (double) panelRadius);

    auto* root = new juce::DynamicObject();
    root->setProperty ("format", kFormat);
    root->setProperty ("version", kVersion);
    root->setProperty ("name", name);
    root->setProperty ("colors", juce::var (colours));
    root->setProperty ("metrics", juce::var (metrics));
    return juce::JSON::toString (juce::var (root), false);
}

// ------------------------------------------------------------------------ manager
ThemeManager::ThemeManager (juce::File themesFolder, juce::File settingsFile)
    : dir (std::move (themesFolder)), settings (std::move (settingsFile))
{
    refresh();
    if (settings.existsAsFile() && settings.getSize() <= 256)
    {
        const int i = indexOf (Theme::cleanName (settings.loadFileAsString()));
        if (i >= 0) currentIndex = i;
    }
}

juce::File ThemeManager::defaultFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Solomon").getChildFile ("Themes");
}

ThemeManager& ThemeManager::shared()
{
    static ThemeManager instance (defaultFolder(), defaultFolder().getSiblingFile ("theme.txt"));
    return instance;
}

int ThemeManager::indexOf (const juce::String& name) const
{
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].name.equalsIgnoreCase (name)) return (int) i;
    return -1;
}

void ThemeManager::refresh()
{
    const auto before = list.empty() ? juce::String() : current().name;
    list = Theme::builtIn();
    builtInCount = (int) list.size();
    skipped.clear();

    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
        files.sort();
        for (const auto& f : files)
        {
            if ((int) list.size() - builtInCount >= kMaxUserThemes) { skipped.add ("too many themes: the rest were ignored"); break; }
            if (f.isSymbolicLink() || f.getSize() > Theme::kMaxBytes)
                { skipped.add (f.getFileName() + ": not a regular file or larger than 8 KiB"); continue; }
            Theme t;
            juce::String error;
            if (! Theme::parse (f.loadFileAsString(), t, error))
                { skipped.add (f.getFileName() + ": " + error); continue; }
            if (indexOf (t.name) >= 0)
                { skipped.add (f.getFileName() + ": a theme called \"" + t.name + "\" already exists"); continue; }
            list.push_back (t);
        }
    }

    const int i = before.isEmpty() ? -1 : indexOf (before);
    currentIndex = i >= 0 ? i : 0;
}

bool ThemeManager::select (const juce::String& name)
{
    const int i = indexOf (name);
    if (i < 0) return false;
    currentIndex = i;
    settings.getParentDirectory().createDirectory();
    settings.replaceWithText (list[(size_t) i].name);           // best effort: a failed save is not an error
    sendSynchronousChangeMessage();
    return true;
}
