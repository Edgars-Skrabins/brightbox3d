#include "Common/TestUtilities.h"
#include "Level/Level.h"
#include "Rendering/DrawCollector.h"
#include "Rendering/FrameLighting.h"
#include "Rendering/Light.h"
#include "Rendering/RenderConstants.h"
#include "Rendering/RenderView.h"
#include "Runtime/RenderContext.h"

#include <gtest/gtest.h>

#include <memory>

using namespace BrightBox3D;
using namespace BrightBox3D::Rendering;

namespace
{
    Light& AddLight(Level::Level& level, LightType type, const Math::Vector3& position)
    {
        Level::LevelObject* object = *level.CreateObject(Level::LevelObject::StaticType());
        object->GetTransform().SetLocalPosition(position);
        Light& light = **object->AddComponent<Light>();
        light.SetLightType(type);
        return light;
    }

    const LightConstants& Constants(const FrameLighting& lighting)
    {
        return *reinterpret_cast<const LightConstants*>(lighting.lightConstants.data());
    }

    class DrawCollectorTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_context = Tests::CreateTestRenderContext();
            ASSERT_NE(m_context, nullptr);
            m_collector = std::make_unique<DrawCollector>(m_context->GetResources(), m_statistics);
        }

        std::unique_ptr<Runtime::RenderContext> m_context;
        RenderStatistics m_statistics;
        std::unique_ptr<DrawCollector> m_collector;
    };
}

TEST_F(DrawCollectorTests, ShadowCastingPointAndSpotLightsBecomeCandidates)
{
    Level::Level level;
    AddLight(level, LightType::Directional, { 0.0f, 5.0f, 0.0f });
    AddLight(level, LightType::Point, { 1.0f, 2.0f, 3.0f }).SetRange(6.0f);
    Light& spot = AddLight(level, LightType::Spot, { -1.0f, 3.0f, 0.0f });
    spot.SetSpotAngles(20.0f, 30.0f);
    spot.SetIntensity(4.0f);
    spot.SetShadowResolution(ShadowResolution::High);
    spot.SetShadowBias(0.002f);
    spot.SetShadowNormalBias(0.75f);
    AddLight(level, LightType::Point, { 0.0f, 1.0f, 0.0f }).SetCastShadows(false);

    FrameLighting lighting;
    m_collector->CollectLights(&level, RenderView(), lighting);

    ASSERT_EQ(lighting.localShadowCandidates.size(), 2u);
    const LocalShadowCandidate& point = lighting.localShadowCandidates[0];
    EXPECT_EQ(point.lightIndex, 1u);
    EXPECT_EQ(point.type, LightType::Point);
    EXPECT_EQ(point.position, Math::Vector3(1.0f, 2.0f, 3.0f));
    EXPECT_FLOAT_EQ(point.range, 6.0f);
    EXPECT_EQ(point.maxTileSize, 1024u);

    const LocalShadowCandidate& candidate = lighting.localShadowCandidates[1];
    EXPECT_EQ(candidate.lightIndex, 2u);
    EXPECT_EQ(candidate.type, LightType::Spot);
    EXPECT_FLOAT_EQ(candidate.outerAngle, 30.0f);
    EXPECT_FLOAT_EQ(candidate.intensity, 4.0f);
    EXPECT_EQ(candidate.maxTileSize, 2048u);
    EXPECT_FLOAT_EQ(candidate.bias, 0.002f);
    EXPECT_FLOAT_EQ(candidate.normalBias, 0.75f);
}

TEST_F(DrawCollectorTests, NoCandidatesWhenLocalShadowsAreOff)
{
    Level::Level level;
    AddLight(level, LightType::Point, { 0.0f, 1.0f, 0.0f });
    FrameLighting lighting;

    RenderView localOff;
    localOff.shadows.localEnabled = false;
    m_collector->CollectLights(&level, localOff, lighting);
    EXPECT_TRUE(lighting.localShadowCandidates.empty());

    RenderView allOff;
    allOff.shadows.enabled = false;
    m_collector->CollectLights(&level, allOff, lighting);
    EXPECT_TRUE(lighting.localShadowCandidates.empty());
}

TEST_F(DrawCollectorTests, CandidatesAreRebuiltEachFrame)
{
    Level::Level level;
    AddLight(level, LightType::Spot, { 0.0f, 1.0f, 0.0f });
    FrameLighting lighting;
    m_collector->CollectLights(&level, RenderView(), lighting);
    m_collector->CollectLights(&level, RenderView(), lighting);
    EXPECT_EQ(lighting.localShadowCandidates.size(), 1u);
}

TEST_F(DrawCollectorTests, EveryLightStartsWithoutALocalShadowIndex)
{
    Level::Level level;
    AddLight(level, LightType::Directional, { 0.0f, 5.0f, 0.0f });
    AddLight(level, LightType::Point, { 0.0f, 1.0f, 0.0f });
    AddLight(level, LightType::Spot, { 0.0f, 1.0f, 0.0f });
    FrameLighting lighting;
    m_collector->CollectLights(&level, RenderView(), lighting);
    ASSERT_EQ(lighting.lightCount, 3u);
    for (uint32_t i = 0; i < lighting.lightCount; ++i)
    {
        EXPECT_FLOAT_EQ(Constants(lighting).lights[i].spotShadow[3], -1.0f) << i;
    }
}
