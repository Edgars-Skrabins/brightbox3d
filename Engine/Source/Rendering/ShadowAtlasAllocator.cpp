#include "Rendering/ShadowAtlasAllocator.h"

#include "Rendering/LocalShadowSelection.h"
#include "Rendering/RenderConstants.h"

#include <algorithm>
#include <bit>

namespace BrightBox3D::Rendering
{
    void ShadowAtlasAllocator::Reset(uint32_t atlasSize)
    {
        m_atlasSize = std::bit_floor(std::max(atlasSize, MinShadowTileSize));
        m_allocatedTileCount = 0;
        m_freeTiles.assign(LevelOf(MinShadowTileSize) + 1, {});
        m_freeTiles[0].push_back({ 0, 0, m_atlasSize });
    }

    std::vector<ShadowTile> ShadowAtlasAllocator::AllocateTiles(uint32_t requestedSize, uint32_t count)
    {
        if (m_freeTiles.empty() || count == 0 || m_allocatedTileCount + count > MaxLocalShadowViews)
        {
            return {};
        }
        for (uint32_t size = std::bit_floor(std::clamp(requestedSize, MinShadowTileSize, m_atlasSize)); size >= MinShadowTileSize; size /= 2)
        {
            std::vector<std::vector<ShadowTile>> before = m_freeTiles;
            std::vector<ShadowTile> tiles;
            while (tiles.size() < count)
            {
                std::optional<ShadowTile> tile = AllocateTile(size);
                if (!tile)
                {
                    break;
                }
                tiles.push_back(*tile);
            }
            if (tiles.size() == count)
            {
                m_allocatedTileCount += count;
                return tiles;
            }
            m_freeTiles = std::move(before);
        }
        return {};
    }

    std::optional<ShadowTile> ShadowAtlasAllocator::AllocateTile(uint32_t size)
    {
        size_t target = LevelOf(size);
        size_t level = target + 1;
        while (level > 0 && m_freeTiles[level - 1].empty())
        {
            --level;
        }
        if (level == 0)
        {
            return std::nullopt;
        }
        --level;
        ShadowTile tile = m_freeTiles[level].back();
        m_freeTiles[level].pop_back();
        while (level < target)
        {
            uint32_t half = tile.size / 2;
            ++level;
            m_freeTiles[level].push_back({ tile.x + half, tile.y + half, half });
            m_freeTiles[level].push_back({ tile.x, tile.y + half, half });
            m_freeTiles[level].push_back({ tile.x + half, tile.y, half });
            tile.size = half;
        }
        return tile;
    }

    size_t ShadowAtlasAllocator::LevelOf(uint32_t size) const
    {
        return static_cast<size_t>(std::bit_width(m_atlasSize) - std::bit_width(size));
    }
}
