#include "Common/TestUtilities.h"
#include "Level/Level.h"
#include "Math/MathUtils.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/Light.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/RenderConstants.h"
#include "Rendering/Renderer.h"
#include "Rendering/TextureCapture.h"
#include "Runtime/RenderContext.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace BrightBox3D;
using namespace BrightBox3D::Rendering;

namespace
{
    constexpr uint32_t OutputWidth = 320;
    constexpr uint32_t OutputHeight = 180;
    constexpr int OpenTolerance = 3;
    constexpr float ShadowedRatio = 0.8f;

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

        int BrightnessAt(const std::vector<uint8_t>& pixels, const Math::Vector3& world) const
        {
            Math::Vector3 ndc = (m_view.view * m_view.projection).TransformPoint(world);
            uint32_t x = static_cast<uint32_t>((ndc.x * 0.5f + 0.5f) * static_cast<float>(OutputWidth));
            uint32_t y = static_cast<uint32_t>((0.5f - ndc.y * 0.5f) * static_cast<float>(OutputHeight));
            size_t index = (static_cast<size_t>(y) * OutputWidth + x) * 4;
            return pixels[index] + pixels[index + 1] + pixels[index + 2];
        }

        std::vector<uint8_t> RenderPixels(bool localShadows, const std::string& name)
        {
            m_view.post.antiAliasing = AntiAliasing::None;
            m_view.shadows.localEnabled = localShadows;
            Render();
            std::vector<uint8_t> pixels;
            EXPECT_TRUE(ReadbackRGBA(m_context->GetDevice(), *m_output, pixels));
            SavePng(m_context->GetDevice(), *m_output, Tests::TestOutputPath(name));
            return pixels;
        }

        void ExpectShadowOnlyBehindTheCube(const Math::Vector3& shadowed, const Math::Vector3& open, const std::string& name)
        {
            std::vector<uint8_t> without = RenderPixels(false, name + "Off.png");
            std::vector<uint8_t> with = RenderPixels(true, name + ".png");
            EXPECT_LT(static_cast<float>(BrightnessAt(with, shadowed)), static_cast<float>(BrightnessAt(without, shadowed)) * ShadowedRatio);
            EXPECT_NEAR(BrightnessAt(with, open), BrightnessAt(without, open), OpenTolerance);
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

TEST_F(LocalShadowPassTests, APointLightDarkensTheFloorBehindTheCube)
{
    Light& light = AddLight(LightType::Point, { -2.5f, 1.0f, 0.0f });
    light.SetIntensity(80.0f);
    light.SetRange(8.0f);
    ExpectShadowOnlyBehindTheCube({ 1.0f, 0.0f, 0.0f }, { -1.5f, 0.0f, -1.5f }, "BrightBox3D_LocalShadowPoint");
}

TEST_F(LocalShadowPassTests, ASpotLightDarkensTheFloorBehindTheCube)
{
    Light& light = AddLight(LightType::Spot, { -2.0f, 3.0f, 0.0f });
    light.SetIntensity(40.0f);
    light.SetRange(8.0f);
    light.SetSpotAngles(50.0f, 60.0f);
    ExpectShadowOnlyBehindTheCube({ 1.2f, 0.0f, 0.0f }, { -2.0f, 0.0f, -1.0f }, "BrightBox3D_LocalShadowSpot");
}
