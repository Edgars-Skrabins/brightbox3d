#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace BrightBox3D::Core
{
    struct Guid
    {
        uint64_t high = 0;
        uint64_t low = 0;

        static Guid Generate();
        static Guid Parse(const std::string& text);

        std::string ToString() const;
        bool IsValid() const { return high != 0 || low != 0; }

        bool operator==(const Guid& other) const { return high == other.high && low == other.low; }
        bool operator!=(const Guid& other) const { return !(*this == other); }
        bool operator<(const Guid& other) const { return high < other.high || (high == other.high && low < other.low); }
    };
}

template <>
struct std::hash<BrightBox3D::Core::Guid>
{
    size_t operator()(const BrightBox3D::Core::Guid& guid) const noexcept
    {
        return static_cast<size_t>(guid.high ^ (guid.low * 0x9E3779B97F4A7C15ull));
    }
};
