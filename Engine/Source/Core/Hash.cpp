#include "Core/Hash.h"

namespace BrightBox3D::Core
{
    uint64_t HashBytes(const void* data, size_t size, uint64_t seed)
    {
        const auto* bytes = static_cast<const unsigned char*>(data);
        uint64_t hash = seed;
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= 0x100000001b3ull;
        }
        return hash;
    }

    uint64_t HashString(std::string_view text)
    {
        return HashBytes(text.data(), text.size());
    }

    uint64_t HashCombine(uint64_t a, uint64_t b)
    {
        return a ^ (b + 0x9E3779B97F4A7C15ull + (a << 6) + (a >> 2));
    }
}
