#include "Engine.h"

#include <algorithm>
#include <locale>
#include <sstream>

namespace engine
{
namespace
{
int toInt (const std::string& s, int fallback)
{
    try { return std::stoi (s); } catch (...) { return fallback; }
}
}

std::string State::serialize() const
{
    std::ostringstream os;
    os.imbue (std::locale::classic());
    os << "SolomonState v1\n";
    os << "gain=" << int (gain * 1000 + 0.5) << "\n";
    os << "mode=" << mode << "\n";
    os << "uiscale=" << int (uiScale * 1000 + 0.5) << "\n";
    return os.str();
}

bool State::deserialize (const std::string& text)
{
    if (text.size() > kMaxStateBytes || text.rfind ("SolomonState", 0) != 0) return false;

    State s;                                     // start from defaults: missing fields stay valid
    std::istringstream is (text);
    std::string line;
    while (std::getline (is, line))
    {
        const auto eq = line.find ('=');
        if (eq == std::string::npos) continue;
        const auto k = line.substr (0, eq), v = line.substr (eq + 1);
        if      (k == "gain")    s.gain = std::clamp (toInt (v, 800) / 1000.0, 0.0, 1.0);
        else if (k == "mode")    s.mode = std::clamp (toInt (v, 0), 0, kNumModes - 1);
        else if (k == "uiscale") s.uiScale = std::clamp (toInt (v, 1000) / 1000.0, 0.6, 2.0);
    }
    *this = s;
    return true;
}
} // namespace engine
