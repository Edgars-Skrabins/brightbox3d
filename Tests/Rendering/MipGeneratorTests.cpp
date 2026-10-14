#include "Math/ColorSpace.h"
#include "Rendering/MipGenerator.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

using namespace BrightBox3D;
using namespace BrightBox3D::Rendering;

namespace
{
    std::vector<std::pair<uint32_t, uint32_t>> Sizes(const std::vector<TextureMip>& mips)
    {
        std::vector<std::pair<uint32_t, uint32_t>> sizes;
        for (const TextureMip& mip : mips)
        {
            sizes.emplace_back(mip.width, mip.height);
        }
        return sizes;
    }

    std::vector<uint8_t> Pixels(std::initializer_list<uint8_t> values)
    {
        return std::vector<uint8_t>(values);
    }
}

TEST(MipGeneratorTests, ChainHalvesEachLevelDownToOnePixel)
{
    std::vector<uint8_t> wide(8 * 4 * 4, 255);
    auto mips = MipGenerator::BuildMips8(wide.data(), 8, 4, false, false, true);
    std::vector<std::pair<uint32_t, uint32_t>> expected = { { 8, 4 }, { 4, 2 }, { 2, 1 }, { 1, 1 } };
    EXPECT_EQ(Sizes(mips), expected);
    for (const TextureMip& mip : mips)
    {
        EXPECT_EQ(mip.pixels.size(), static_cast<size_t>(mip.width) * mip.height * 4);
    }

    std::vector<uint8_t> odd(5 * 3 * 4, 255);
    expected = { { 5, 3 }, { 2, 1 }, { 1, 1 } };
    EXPECT_EQ(Sizes(MipGenerator::BuildMips8(odd.data(), 5, 3, false, false, true)), expected);
}

TEST(MipGeneratorTests, DisabledGenerationKeepsOnlyTheSourcePixels)
{
    std::vector<uint8_t> pixels = Pixels({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 });
    auto mips = MipGenerator::BuildMips8(pixels.data(), 2, 2, true, false, false);
    ASSERT_EQ(mips.size(), 1u);
    EXPECT_EQ(mips[0].pixels, pixels);
}

TEST(MipGeneratorTests, LinearTexelsAverageWithRounding)
{
    std::vector<uint8_t> pixels = Pixels({ 0, 10, 200, 0, 255, 20, 200, 255, 0, 30, 100, 0, 255, 40, 100, 255 });
    auto mips = MipGenerator::BuildMips8(pixels.data(), 2, 2, false, false, true);
    ASSERT_EQ(mips.size(), 2u);
    const std::vector<uint8_t>& average = mips[1].pixels;
    EXPECT_EQ(average[0], 128);
    EXPECT_EQ(average[1], 25);
    EXPECT_EQ(average[2], 150);
    EXPECT_EQ(average[3], 128);
}

TEST(MipGeneratorTests, SrgbTexelsAverageInLinearSpace)
{
    std::vector<uint8_t> pixels = Pixels({ 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255 });
    auto mips = MipGenerator::BuildMips8(pixels.data(), 2, 2, true, false, true);
    uint8_t expected = static_cast<uint8_t>(Math::LinearToSrgb(0.5f) * 255.0f + 0.5f);
    EXPECT_EQ(expected, 188);
    EXPECT_EQ(mips[1].pixels[0], expected);
    EXPECT_EQ(mips[1].pixels[2], expected);
    EXPECT_EQ(mips[1].pixels[3], 128);
}

TEST(MipGeneratorTests, NormalMapAveragesAreRenormalized)
{
    std::vector<uint8_t> pixels = Pixels({ 255, 128, 128, 255, 128, 128, 255, 255 });
    auto mips = MipGenerator::BuildMips8(pixels.data(), 2, 1, false, true, true);
    ASSERT_EQ(mips.size(), 2u);
    const std::vector<uint8_t>& normal = mips[1].pixels;
    EXPECT_EQ(normal[0], 218);
    EXPECT_EQ(normal[2], 218);
    float x = normal[0] / 255.0f * 2.0f - 1.0f;
    float y = normal[1] / 255.0f * 2.0f - 1.0f;
    float z = normal[2] / 255.0f * 2.0f - 1.0f;
    EXPECT_NEAR(std::sqrt(x * x + y * y + z * z), 1.0f, 0.02f);
}

TEST(MipGeneratorTests, FloatTexelsAverageExactly)
{
    std::vector<float> pixels = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, -1.0f, 0.0f, 10.0f, 0.5f, 3.0f, 4.0f, 0.0f, 1.5f };
    auto mips = MipGenerator::BuildMipsFloat(pixels.data(), 2, 2, true);
    ASSERT_EQ(mips.size(), 2u);
    ASSERT_EQ(mips[1].pixels.size(), 4 * sizeof(float));
    float average[4];
    std::memcpy(average, mips[1].pixels.data(), sizeof(average));
    EXPECT_FLOAT_EQ(average[0], 2.0f);
    EXPECT_FLOAT_EQ(average[1], 3.0f);
    EXPECT_FLOAT_EQ(average[2], 5.0f);
    EXPECT_FLOAT_EQ(average[3], 3.5f);
}
