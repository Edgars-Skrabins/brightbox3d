#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace BrightBox3D::Core
{
    uint64_t HashBytes(const void* data, size_t size, uint64_t seed = 0xcbf29ce484222325ull);
    uint64_t HashString(std::string_view text);
    uint64_t HashCombine(uint64_t a, uint64_t b);
}
