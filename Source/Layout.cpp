#include "Layout.h"

#include <algorithm>
#include <cmath>

namespace
{
int bracketCount (const juce::String& s)
{
    int n = 0;
    for (auto c : s) if (c == '{' || c == '[') ++n;
    return n;
}

bool readInt (const juce::var& v, int& out)
{
    if (! (v.isInt() || v.isInt64() || v.isDouble())) return false;
    const double d = (double) v;
    if (! std::isfinite (d)) return false;
    out = (int) juce::jlimit (-1.0e6, 1.0e6, std::round (d));
    return true;
}
} // namespace

bool Layout::isValidId (const juce::String& id)
{
    if (id.isEmpty() || id.length() > kMaxIdLength) return false;
    for (auto c : id)
        if (! (juce::CharacterFunctions::isLetterOrDigit (c) && c < 128) && c != '.' && c != '_' && c != '-') return false;
    return true;
}

juce::String Layout::cleanName (const juce::String& raw)
{
    juce::String out;
    for (auto c : raw)
        if (c >= 32 && c != 127) out += c;
    return out.trim().substring (0, kMaxNameLength).trim();
}

bool Layout::parse (const juce::String& json, const juce::String& pluginName, int baseWidth, int baseHeight,
                    Layout& out, juce::String& error)
{
    if (json.getNumBytesAsUTF8() > (size_t) kMaxBytes) { error = "file is larger than 32 KiB"; return false; }
    if (bracketCount (json) > kMaxEntries + 16)        { error = "too many nested objects"; return false; }

    juce::var root;
    if (juce::JSON::parse (json, root).failed())        { error = "not valid JSON"; return false; }
    auto* obj = root.getDynamicObject();
    if (obj == nullptr)                                 { error = "the top level must be an object"; return false; }

    if (obj->getProperty ("format").toString() != kFormat) { error = "missing \"format\": \"plugin-layout\""; return false; }
    const auto& version = obj->getProperty ("version");
    if (! (version.isInt() || version.isInt64()))       { error = "missing integer \"version\""; return false; }
    if ((int) version != kVersion)                      { error = "unsupported version " + version.toString(); return false; }
    if (obj->getProperty ("plugin").toString() != pluginName)
                                                        { error = "this layout was made for another plug-in"; return false; }
    int fileW = 0, fileH = 0;
    if (! readInt (obj->getProperty ("baseWidth"), fileW) || ! readInt (obj->getProperty ("baseHeight"), fileH))
                                                        { error = "missing \"baseWidth\" / \"baseHeight\""; return false; }
    if (fileW != baseWidth || fileH != baseHeight)
                                                        { error = "made for a different window size (" + juce::String (fileW) + "x" + juce::String (fileH) + ")"; return false; }

    Layout l;
    l.name = cleanName (obj->getProperty ("name").toString());
    if (l.name.isEmpty())                               { error = "missing \"name\""; return false; }

    const auto& elements = obj->getProperty ("elements");
    auto* map = elements.getDynamicObject();
    if (map == nullptr)                                 { error = "\"elements\" must be an object"; return false; }
    if (map->getProperties().size() > kMaxEntries)      { error = "too many elements"; return false; }

    for (const auto& prop : map->getProperties())
    {
        const juce::String id = prop.name.toString();
        if (! isValidId (id))                           { error = "bad element id"; return false; }
        auto* e = prop.value.getDynamicObject();
        int x = 0, y = 0, w = 0, h = 0;
        if (e == nullptr || ! readInt (e->getProperty ("x"), x) || ! readInt (e->getProperty ("y"), y)
            || ! readInt (e->getProperty ("w"), w) || ! readInt (e->getProperty ("h"), h))
                                                        { error = "element \"" + id + "\" needs numbers x, y, w and h"; return false; }
        Entry entry;
        w = juce::jlimit (kMinSize, baseWidth, w);
        h = juce::jlimit (kMinSize, baseHeight, h);
        entry.bounds = { juce::jlimit (0, baseWidth - w, x), juce::jlimit (0, baseHeight - h, y), w, h };
        const auto& vis = e->getProperty ("visible");
        if (! vis.isVoid())
        {
            if (! vis.isBool())                         { error = "\"visible\" of \"" + id + "\" must be true or false"; return false; }
            entry.visible = (bool) vis;
        }
        l.entries[id] = entry;
    }

    out = std::move (l);
    return true;
}

juce::String Layout::toJson (const juce::String& pluginName, int baseWidth, int baseHeight) const
{
    auto* elements = new juce::DynamicObject();
    for (const auto& [id, e] : entries)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("x", e.bounds.getX());      o->setProperty ("y", e.bounds.getY());
        o->setProperty ("w", e.bounds.getWidth());  o->setProperty ("h", e.bounds.getHeight());
        if (! e.visible) o->setProperty ("visible", false);
        elements->setProperty (juce::Identifier (id), juce::var (o));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", kFormat);
    root->setProperty ("version", kVersion);
    root->setProperty ("plugin", pluginName);
    root->setProperty ("name", name);
    root->setProperty ("baseWidth", baseWidth);
    root->setProperty ("baseHeight", baseHeight);
    root->setProperty ("elements", juce::var (elements));
    return juce::JSON::toString (juce::var (root), false);
}

// ------------------------------------------------------------------------ manager
LayoutManager::LayoutManager (juce::String pluginName, int baseWidth, int baseHeight, juce::File layoutsFolder, juce::File settingsFile)
    : plugin (std::move (pluginName)), baseW (baseWidth), baseH (baseHeight),
      dir (std::move (layoutsFolder)), settings (std::move (settingsFile))
{
    refresh();
    if (settings.existsAsFile() && settings.getSize() <= 256)
    {
        const int i = indexOf (Layout::cleanName (settings.loadFileAsString()));
        if (i >= 0) currentIndex = i;
    }
}

int LayoutManager::indexOf (const juce::String& name) const
{
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].name.equalsIgnoreCase (name)) return (int) i;
    return -1;
}

void LayoutManager::refresh()
{
    const auto before = list.empty() ? juce::String() : current().name;
    list.assign (1, Layout());                       // "Default": no overrides
    skipped.clear();

    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
        files.sort();
        for (const auto& f : files)
        {
            if ((int) list.size() - 1 >= kMaxUserLayouts) { skipped.add ("too many layouts: the rest were ignored"); break; }
            if (f.isSymbolicLink() || f.getSize() > Layout::kMaxBytes)
                { skipped.add (f.getFileName() + ": not a regular file or larger than 32 KiB"); continue; }
            Layout l;
            juce::String error;
            if (! Layout::parse (f.loadFileAsString(), plugin, baseW, baseH, l, error))
                { skipped.add (f.getFileName() + ": " + error); continue; }
            if (indexOf (l.name) >= 0)
                { skipped.add (f.getFileName() + ": a layout called \"" + l.name + "\" already exists"); continue; }
            list.push_back (std::move (l));
        }
    }

    const int i = before.isEmpty() ? -1 : indexOf (before);
    currentIndex = i >= 0 ? i : 0;
}

bool LayoutManager::select (const juce::String& name)
{
    const int i = indexOf (name);
    if (i < 0) return false;
    currentIndex = i;
    settings.getParentDirectory().createDirectory();
    settings.replaceWithText (list[(size_t) i].name);
    sendSynchronousChangeMessage();
    return true;
}
