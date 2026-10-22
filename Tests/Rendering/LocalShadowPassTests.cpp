#include "Common/TestUtilities.h"
#include "Level/Level.h"
#include "Math/MathUtils.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/Light.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/RenderConstants.h"
#include "Rendering/Renderer.h"
#include "Runtime/RenderContext.h"

#include <gtest/gtest.h>

#include <memory>

using namespace BrightBox3D;
using namespace BrightBox3D::Rendering;

namespace
{
    constexpr uint32_t OutputWidth = 320;
    constexpr uint32_t OutputHeight = 180;

    class LocalShadowPassTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_context = Tests::CreateTestRenderContext();
            ASSERT_NE(m_context, nullptr);
            RHI::TextureDesc desc;
            desc.width = OutputWidth;
            desc.height = OutputHeight;
            desc.format = RHI::Format::RGBA8Unorm;
            desc.usage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
            m_output = m_context->GetDevice().CreateTexture(desc);

            AddMesh(BuiltinAssets::Plane(), { 0.0f, 0.0f, 0.0f }, { 2.0f, 1.0f, 2.0f });
            AddMesh(BuiltinAssets::Cube(), { 0.0f, 0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f });

            m_view.id = 1;
            m_view.width = OutputWidth;
            m_view.height = OutputHeight;
            m_view.position = { 0.0f, 3.0f, -7.0f };
            m_view.view = Math::Matrix4::LookAtLH(m_view.position, { 0.0f, 0.5f, 0.0f }, Math::Vector3::Up());
            m_view.projection = Math::Matrix4::PerspectiveFovLH(m_view.verticalFov * Math::DegreesToRadians, static_cast<float>(OutputWidth) / static_cast<float>(OutputHeight), m_view.nearPlane, m_view.farPlane);
        }

        void AddMesh(const Core::Guid& model, const Math::Vector3& position, const Math::Vector3& scale)
        {
            Level::LevelObject* object = *m_level.CreateObject(Level::LevelObject::StaticType());
            object->GetTransform().SetLocalPosition(position);
            object->GetTransform().SetLocalScale(scale);
            auto* renderer = *object->AddComponent<MeshRenderer>();
            renderer->SetModel(Assets::AssetReference<Geometry::ModelAsset>(model));
        }

        Light& AddLight(LightType type, const Math::Vector3& position)
        {
            Level::LevelObject* object = *m_level.CreateObject(Level::LevelObject::StaticType());
            object->GetTransform().SetLocalPosition(position);
            object->GetTransform().SetLocalEulerAngles({ 90.0f, 0.0f, 0.0f });
            Light& light = **object->AddComponent<Light>();
            light.SetLightType(type);
            light.SetIntensity(8.0f);
            light.SetRange(6.0f);
            return light;
        }

        const RenderStatistics& Render()
        {
            RHI::Device& device = m_context->GetDevice();
            RHI::CommandList& commands = device.BeginFrame();
            m_context->GetRenderer().Render(commands, &m_level, m_view, m_output.get());
            device.EndFrame({});
            return m_context->GetRenderer().GetStatistics();
        }

        std::unique_ptr<Runtime::RenderContext> m_context;
        std::unique_ptr<RHI::Texture> m_output;
        Level::Level m_level;
        RenderView m_view;
    };
}

TEST_F(LocalShadowPassTests, ASpotAndAPointLightRenderSevenViews)
{
    AddLight(LightType::Spot, { 0.0f, 3.0f, 0.0f });
    AddLight(LightType::Point, { 2.0f, 1.5f, -1.0f });
    const RenderStatistics& statistics = Render();
    EXPECT_EQ(statistics.localShadowLights, 2u);
    EXPECT_EQ(statistics.localShadowViews, 7u);
    EXPECT_GT(statistics.shadowCasters, 0u);
}

TEST_F(LocalShadowPassTests, NoViewsWhenLocalShadowsAreOff)
{
    AddLight(LightType::Spot, { 0.0f, 3.0f, 0.0f });
    AddLight(LightType::Point, { 2.0f, 1.5f, -1.0f });
    m_view.shadows.localEnabled = false;
    const RenderStatistics& statistics = Render();
    EXPECT_EQ(statistics.localShadowLights, 0u);
    EXPECT_EQ(statistics.localShadowViews, 0u);
}

TEST_F(LocalShadowPassTests, NoViewsForLightsThatDoNotCastShadows)
{
    AddLight(LightType::Spot, { 0.0f, 3.0f, 0.0f }).SetCastShadows(false);
    AddLight(LightType::Point, { 2.0f, 1.5f, -1.0f }).SetCastShadows(false);
    const RenderStatistics& statistics = Render();
    EXPECT_EQ(statistics.localShadowLights, 0u);
    EXPECT_EQ(statistics.localShadowViews, 0u);
}

TEST_F(LocalShadowPassTests, PointLightsStopAtTheViewCap)
{
    uint32_t fitting = MaxLocalShadowViews / 6;
    for (uint32_t i = 0; i <= fitting; ++i)
    {
        AddLight(LightType::Point, { -2.5f + 0.5f * static_cast<float>(i), 1.5f, 0.0f });
    }
    const RenderStatistics& statistics = Render();
    EXPECT_EQ(statistics.localShadowLights, fitting);
    EXPECT_EQ(statistics.localShadowViews, fitting * 6);
}
