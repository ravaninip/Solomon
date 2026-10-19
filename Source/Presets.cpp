#include "Presets.h"
#include "StateGuard.h"

namespace
{
struct Value { const char* id; float value; };
struct Factory { const char* name; std::vector<Value> values; };

// Starting points, not finished sounds: they were built from parameter knowledge and level-checked by
// the automated tests, but never auditioned, so expect to adjust them by ear.
// Parameter value ranges are those of the parameters (choice parameters take their index).
const std::vector<Factory>& factoryPresets()
{
    static const std::vector<Factory> presets = {
        { "Init", {} },
        { "Analog Bass", { { "model", 5 }, { "harmonics", 0.2f }, { "timbre", 0.15f }, { "morph", 0.8f },
                           { "cutoff", 420 }, { "resonance", 0.35f }, { "ampD", 0.35f }, { "ampS", 0.7f }, { "ampR", 0.12f },
                           { "fltD", 0.28f }, { "fltS", 0.05f }, { "mod_env_cutoff", 0.55f }, { "tune", -12 } } },
        { "Sync Lead", { { "model", 5 }, { "harmonics", 0.35f }, { "timbre", 0.7f }, { "morph", 0.45f },
                         { "cutoff", 5200 }, { "resonance", 0.25f }, { "ampA", 0.01f }, { "ampS", 0.9f }, { "ampR", 0.2f },
                         { "lfoRate", 5.2f }, { "mod_lfo_timbre", 0.12f }, { "mod_pres_timbre", 0.35f } } },
        { "Warm Strings", { { "model", 3 }, { "harmonics", 0.0f }, { "timbre", 0.55f }, { "morph", 0.45f },
                            { "cutoff", 6500 }, { "ampA", 0.55f }, { "ampD", 0.8f }, { "ampS", 0.85f }, { "ampR", 1.4f },
                            { "lfoRate", 0.35f }, { "mod_lfo_morph", 0.15f } } },
        { "Glass Keys", { { "model", 7 }, { "harmonics", 0.55f }, { "timbre", 0.35f }, { "morph", 0.5f },
                          { "cutoff", 14000 }, { "ampA", 0.002f }, { "ampD", 1.1f }, { "ampS", 0.0f }, { "ampR", 0.9f },
                          { "fltD", 0.4f }, { "mod_env_cutoff", 0.0f }, { "mod_env_timbre", 0.35f }, { "mod_vel_timbre", 0.3f } } },
        { "Bell Tower", { { "model", 16 }, { "harmonics", 0.45f }, { "timbre", 0.7f }, { "morph", 0.65f },
                          { "cutoff", 16000 }, { "ampA", 0.002f }, { "ampD", 2.5f }, { "ampS", 0.4f }, { "ampR", 2.5f },
                          { "mod_env_cutoff", 0.0f }, { "mod_vel_timbre", 0.3f } } },
        { "Pluck", { { "model", 15 }, { "harmonics", 0.3f }, { "timbre", 0.55f }, { "morph", 0.6f },
                     { "cutoff", 12000 }, { "ampA", 0.001f }, { "ampD", 1.2f }, { "ampS", 0.5f }, { "ampR", 0.6f },
                     { "mod_env_cutoff", 0.0f }, { "mod_vel_timbre", 0.4f } } },
        { "Wavetable Sweep", { { "model", 10 }, { "harmonics", 0.2f }, { "timbre", 0.3f }, { "morph", 0.5f },
                               { "cutoff", 9000 }, { "ampA", 0.05f }, { "ampS", 0.9f }, { "ampR", 0.6f },
                               { "lfoRate", 0.25f }, { "lfoShape", 1 }, { "mod_lfo_timbre", 0.55f }, { "mod_lfo_morph", 0.3f },
                               { "mod_env_cutoff", 0.0f } } },
        { "Organ Chords", { { "model", 11 }, { "harmonics", 0.25f }, { "timbre", 0.3f }, { "morph", 0.15f },
                            { "cutoff", 7500 }, { "ampA", 0.01f }, { "ampS", 1.0f }, { "ampR", 0.15f },
                            { "mod_env_cutoff", 0.0f }, { "mod_pres_harm", 0.5f } } },
        { "Chip Arp", { { "model", 4 }, { "harmonics", 0.4f }, { "timbre", 0.6f }, { "morph", 0.3f },
                        { "cutoff", 12000 }, { "ampA", 0.001f }, { "ampS", 1.0f }, { "ampR", 0.08f },
                        { "mod_env_cutoff", 0.0f } } },
        { "Swarm Cloud", { { "model", 12 }, { "harmonics", 0.45f }, { "timbre", 0.5f }, { "morph", 0.55f },
                           { "cutoff", 4800 }, { "resonance", 0.2f }, { "ampA", 1.2f }, { "ampD", 1.0f }, { "ampS", 0.9f }, { "ampR", 2.2f },
                           { "lfoRate", 0.18f }, { "mod_lfo_cutoff", 0.35f }, { "mod_env_cutoff", 0.0f } } },
        { "Wind", { { "model", 13 }, { "harmonics", 0.5f }, { "timbre", 0.35f }, { "morph", 0.4f },
                    { "cutoff", 3500 }, { "resonance", 0.3f }, { "ampA", 1.5f }, { "ampS", 1.0f }, { "ampR", 2.0f },
                    { "lfoRate", 0.3f }, { "lfoShape", 4 }, { "mod_lfo_morph", 0.25f }, { "mod_lfo_cutoff", 0.3f }, { "mod_env_cutoff", 0.0f } } },
        { "Pressure Pad", { { "model", 0 }, { "harmonics", 0.5f }, { "timbre", 0.4f }, { "morph", 0.35f },
                            { "cutoff", 1800 }, { "resonance", 0.3f }, { "ampA", 0.4f }, { "ampD", 0.8f }, { "ampS", 0.85f }, { "ampR", 1.2f },
                            { "mod_env_cutoff", 0.15f }, { "mod_pres_cutoff", 0.7f }, { "mod_pres_timbre", 0.3f } } },
    };
    return presets;
}

juce::File readableFile (const juce::File& f)
{
    return f.existsAsFile() ? f : juce::File();
}
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state, juce::File userFolder)
    : apvts (state), userDir (std::move (userFolder))
{
    refresh (false);          // no disk access while the plug-in is being created
}

juce::File PresetManager::defaultUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Solomon").getChildFile ("Presets");
}

juce::String PresetManager::cleanName (const juce::String& raw)
{
    auto n = juce::File::createLegalFileName (raw.trim()).trim();
    while (n.startsWithChar ('.')) n = n.substring (1);          // no hidden files
    return n.substring (0, kMaxNameLength).trim();
}

void PresetManager::refresh (bool scanUserFolder)
{
    list.clear();
    for (const auto& f : factoryPresets())
        list.push_back ({ f.name, true, {} });
    factoryCount = (int) list.size();

    if (! scanUserFolder || ! userDir.isDirectory()) return;
    auto files = userDir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
    files.sort();
    for (const auto& f : files)
    {
        if ((int) list.size() - factoryCount >= kMaxUserPresets) break;
        list.push_back ({ f.getFileNameWithoutExtension(), false, f });
    }
}

void PresetManager::applyFactory (int index)
{
    for (auto* prm : apvts.processor.getParameters())
        prm->setValueNotifyingHost (prm->getDefaultValue());

    for (const auto& v : factoryPresets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (v.id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v.value));
}

bool PresetManager::load (int index)
{
    if (index < 0 || index >= (int) list.size()) return false;
    const auto& e = list[(size_t) index];

    if (e.factory)
    {
        applyFactory (index);
        apvts.state.setProperty ("preset", e.name, nullptr);
        return true;
    }

    // User presets are files anyone can edit: apply the same checks as for a host session.
    const auto file = readableFile (e.file);
    if (file == juce::File() || file.getSize() <= 0 || file.getSize() > (juce::int64) solomon::kMaxStateBytes)
        return false;
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data) || ! solomon::isPlausibleState (data.getData(), data.getSize()))
        return false;
    auto xml = juce::parseXML (data.toString());
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    apvts.state.setProperty ("preset", e.name, nullptr);
    return true;
}

bool PresetManager::save (const juce::String& name)
{
    const auto clean = cleanName (name);
    if (clean.isEmpty()) return false;
    if (! userDir.createDirectory().wasOk()) return false;

    const auto target = userDir.getChildFile (clean + kExtension);
    if (target.getParentDirectory() != userDir) return false;                  // never write outside the folder
    for (const auto& f : factoryPresets())
        if (clean.equalsIgnoreCase (f.name)) return false;                      // keep factory names unique

    auto state = apvts.copyState();
    state.setProperty ("preset", clean, nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr || ! target.replaceWithText (xml->toString())) return false;

    apvts.state.setProperty ("preset", clean, nullptr);
    refresh();
    return true;
}

bool PresetManager::remove (int index)
{
    if (index < factoryCount || index >= (int) list.size()) return false;
    const bool ok = list[(size_t) index].file.deleteFile();
    refresh();
    return ok;
}

juce::String PresetManager::validateFactory() const
{
    for (const auto& preset : factoryPresets())
        for (const auto& v : preset.values)
        {
            auto* prm = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (v.id));
            if (prm == nullptr)
                return juce::String (preset.name) + ": unknown parameter '" + v.id + "'";
            const auto range = prm->getNormalisableRange();
            if (v.value < range.start || v.value > range.end)
                return juce::String (preset.name) + ": value out of range for '" + v.id + "'";
        }
    return {};
}

juce::String PresetManager::currentName() const
{
    return apvts.state.getProperty ("preset").toString().substring (0, kMaxNameLength);
}

int PresetManager::indexOfCurrent() const
{
    const auto name = currentName();
    if (name.isEmpty()) return -1;
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].name == name) return i;
    return -1;
}
