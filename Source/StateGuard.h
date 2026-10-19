#pragma once
#include <cstddef>

namespace solomon
{
// Sessions are stored by the host and can come from anywhere, so a state blob is checked before
// it reaches the XML parser: a legitimate Solomon state is a few kB with a flat structure, while a
// crafted one could be huge or nested thousands of levels deep (the parser is recursive).
constexpr size_t kMaxStateBytes = 64 * 1024;
constexpr size_t kMaxStateTags = 512;

inline bool isPlausibleState (const void* data, size_t size)
{
    if (data == nullptr || size == 0 || size > kMaxStateBytes) return false;
    const auto* bytes = static_cast<const unsigned char*> (data);
    size_t tags = 0;
    for (size_t i = 0; i < size; ++i)
        if (bytes[i] == '<' && ++tags > kMaxStateTags) return false;
    return true;
}
} // namespace solomon
