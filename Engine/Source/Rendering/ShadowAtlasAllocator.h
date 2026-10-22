#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace BrightBox3D::Rendering
{
    struct ShadowTile
    {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t size = 0;
    };

    class ShadowAtlasAllocator
    {
    public:
        void Reset(uint32_t atlasSize);
        std::vector<ShadowTile> AllocateTiles(uint32_t requestedSize, uint32_t count);
        uint32_t GetAllocatedTileCount() const { return m_allocatedTileCount; }

    private:
        std::optional<ShadowTile> AllocateTile(uint32_t size);
        size_t LevelOf(uint32_t size) const;

        uint32_t m_atlasSize = 0;
        uint32_t m_allocatedTileCount = 0;
        std::vector<std::vector<ShadowTile>> m_freeTiles;
    };
}
