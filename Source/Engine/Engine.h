#pragma once
#include <cstddef>
#include <string>

namespace engine
{
// Limits applied to state loaded from a host session, which may come from an untrusted file.
constexpr size_t kMaxStateBytes = 1 << 20;

// Complete user state. No JUCE dependency: serialisable to text and unit-testable.
struct State
{
    double gain = 0.8;          // 0..1
    int mode = 0;               // 0..kNumModes-1
    double uiScale = 1.0;       // 0.6..2.0

    static constexpr int kNumModes = 3;

    std::string serialize() const;
    // Returns false (and leaves *this untouched) for anything that is not a valid state.
    // Every field is range-checked: never trust the host's blob.
    bool deserialize (const std::string& text);
};
} // namespace engine
