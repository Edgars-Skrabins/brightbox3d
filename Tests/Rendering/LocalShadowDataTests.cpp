#include "Common/TestUtilities.h"
#include "Rendering/FrameLighting.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/RenderConstants.h"
#include "Runtime/RenderContext.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>

using namespace BrightBox3D;
using namespace BrightBox3D::Rendering;

namespace
{
    constexpr size_t ShaderRegisterSize = 16;
}

TEST(LocalShadowDataTests, ConstantsPackIntoWholeShaderRegisters)
{
    EXPECT_EQ(sizeof(LocalShadowView) % ShaderRegisterSize, 0u);
    EXPECT_EQ(sizeof(LocalShadowView), sizeof(Math::Matrix4) + 2 * ShaderRegisterSize);
    EXPECT_EQ(offsetof(LocalShadowConstants, views), ShaderRegisterSize);
    EXPECT_EQ(sizeof(LocalShadowConstants), ShaderRegisterSize + sizeof(LocalShadowView) * MaxLocalShadowViews);
}

TEST(LocalShadowDataTests, FrameLightingHoldsTheLocalShadowConstants)
{
    FrameLighting lighting;
    EXPECT_EQ(lighting.localShadowConstants.size(), sizeof(LocalShadowConstants));
    EXPECT_EQ(lighting.localShadowAtlas, nullptr);
}

TEST(LocalShadowDataTests, TheDummyAtlasIsASingleDepthTexel)
{
    std::unique_ptr<Runtime::RenderContext> context = Tests::CreateTestRenderContext();
    ASSERT_NE(context, nullptr);
    RHI::Texture* atlas = context->GetResources().GetDummyLocalShadowAtlas();
    ASSERT_NE(atlas, nullptr);
    EXPECT_EQ(atlas->GetDesc().type, RHI::TextureType::Texture2D);
    EXPECT_EQ(atlas->GetFormat(), RHI::Format::Depth32Float);
    EXPECT_EQ(atlas->GetWidth(), 1u);
    EXPECT_EQ(atlas->GetHeight(), 1u);
}
