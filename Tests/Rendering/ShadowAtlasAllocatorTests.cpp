#include "Rendering/LocalShadowSelection.h"
#include "Rendering/RenderConstants.h"
#include "Rendering/ShadowAtlasAllocator.h"

#include <gtest/gtest.h>

#include <vector>

using namespace BrightBox3D::Rendering;

namespace
{
    bool Overlap(const ShadowTile& a, const ShadowTile& b)
    {
        return a.x < b.x + b.size && b.x < a.x + a.size && a.y < b.y + b.size && b.y < a.y + a.size;
    }

    void Append(std::vector<ShadowTile>& all, const std::vector<ShadowTile>& tiles)
    {
        all.insert(all.end(), tiles.begin(), tiles.end());
    }
}

TEST(ShadowAtlasAllocatorTests, FirstTileStartsAtTheOrigin)
{
    ShadowAtlasAllocator allocator;
    allocator.Reset(4096);
    std::vector<ShadowTile> tiles = allocator.AllocateTiles(1024, 1);
    ASSERT_EQ(tiles.size(), 1u);
    EXPECT_EQ(tiles[0].x, 0u);
    EXPECT_EQ(tiles[0].y, 0u);
    EXPECT_EQ(tiles[0].size, 1024u);
    EXPECT_EQ(allocator.GetAllocatedTileCount(), 1u);
}

TEST(ShadowAtlasAllocatorTests, TilesNeverOverlapAndStayAlignedInsideTheAtlas)
{
    ShadowAtlasAllocator allocator;
    allocator.Reset(4096);
    std::vector<ShadowTile> all;
    Append(all, allocator.AllocateTiles(1024, 6));
    Append(all, allocator.AllocateTiles(2048, 1));
    Append(all, allocator.AllocateTiles(512, 6));
    Append(all, allocator.AllocateTiles(256, 1));
    Append(all, allocator.AllocateTiles(128, 6));
    ASSERT_EQ(all.size(), 20u);
    for (size_t i = 0; i < all.size(); ++i)
    {
        const ShadowTile& tile = all[i];
        EXPECT_EQ(tile.x % tile.size, 0u);
        EXPECT_EQ(tile.y % tile.size, 0u);
        EXPECT_LE(tile.x + tile.size, 4096u);
        EXPECT_LE(tile.y + tile.size, 4096u);
        for (size_t j = i + 1; j < all.size(); ++j)
        {
            EXPECT_FALSE(Overlap(tile, all[j])) << i << " and " << j;
        }
    }
}

TEST(ShadowAtlasAllocatorTests, AFullAtlasHalvesTheSizeBeforeGivingUp)
{
    ShadowAtlasAllocator allocator;
    allocator.Reset(4096);
    ASSERT_EQ(allocator.AllocateTiles(2048, 3).size(), 3u);
    ASSERT_EQ(allocator.AllocateTiles(1024, 3).size(), 3u);
    std::vector<ShadowTile> tiles = allocator.AllocateTiles(1024, 2);
    ASSERT_EQ(tiles.size(), 2u);
    EXPECT_EQ(tiles[0].size, 512u);
    EXPECT_EQ(tiles[1].size, 512u);
}

TEST(ShadowAtlasAllocatorTests, AllTilesOfARequestShareOneSize)
{
    ShadowAtlasAllocator allocator;
    allocator.Reset(4096);
    ASSERT_EQ(allocator.AllocateTiles(2048, 3).size(), 3u);
    std::vector<ShadowTile> tiles = allocator.AllocateTiles(2048, 6);
    ASSERT_EQ(tiles.size(), 6u);
    for (const ShadowTile& tile : tiles)
    {
        EXPECT_EQ(tile.size, 512u);
    }
}

TEST(ShadowAtlasAllocatorTests, ARequestThatDoesNotFitLeavesTheAtlasUnchanged)
{
    ShadowAtlasAllocator allocator;
    allocator.Reset(256);
    EXPECT_TRUE(allocator.AllocateTiles(MinShadowTileSize, 6).empty());
    EXPECT_EQ(allocator.GetAllocatedTileCount(), 0u);
    EXPECT_EQ(allocator.AllocateTiles(MinShadowTileSize, 4).size(), 4u);
    EXPECT_TRUE(allocator.AllocateTiles(MinShadowTileSize, 1).empty());
}

TEST(ShadowAtlasAllocatorTests, RequestsBelowTheMinimumUseTheMinimum)
{
    ShadowAtlasAllocator allocator;
    allocator.Reset(4096);
    std::vector<ShadowTile> tiles = allocator.AllocateTiles(16, 1);
    ASSERT_EQ(tiles.size(), 1u);
    EXPECT_EQ(tiles[0].size, MinShadowTileSize);
}

TEST(ShadowAtlasAllocatorTests, NoFrameExceedsTheViewCap)
{
    ShadowAtlasAllocator allocator;
    allocator.Reset(8192);
    uint32_t pointLights = MaxLocalShadowViews / 6;
    for (uint32_t i = 0; i < pointLights; ++i)
    {
        ASSERT_EQ(allocator.AllocateTiles(MinShadowTileSize, 6).size(), 6u);
    }
    EXPECT_TRUE(allocator.AllocateTiles(MinShadowTileSize, 6).empty());
    for (uint32_t i = pointLights * 6; i < MaxLocalShadowViews; ++i)
    {
        ASSERT_EQ(allocator.AllocateTiles(MinShadowTileSize, 1).size(), 1u);
    }
    EXPECT_TRUE(allocator.AllocateTiles(MinShadowTileSize, 1).empty());
    EXPECT_EQ(allocator.GetAllocatedTileCount(), MaxLocalShadowViews);
}

TEST(ShadowAtlasAllocatorTests, ResetFreesTheWholeAtlas)
{
    ShadowAtlasAllocator allocator;
    allocator.Reset(4096);
    ASSERT_EQ(allocator.AllocateTiles(4096, 1).size(), 1u);
    EXPECT_TRUE(allocator.AllocateTiles(MinShadowTileSize, 1).empty());
    allocator.Reset(4096);
    EXPECT_EQ(allocator.GetAllocatedTileCount(), 0u);
    std::vector<ShadowTile> tiles = allocator.AllocateTiles(4096, 1);
    ASSERT_EQ(tiles.size(), 1u);
    EXPECT_EQ(tiles[0].size, 4096u);
}

TEST(ShadowAtlasAllocatorTests, NothingIsAllocatedBeforeTheFirstReset)
{
    ShadowAtlasAllocator allocator;
    EXPECT_TRUE(allocator.AllocateTiles(1024, 1).empty());
}
