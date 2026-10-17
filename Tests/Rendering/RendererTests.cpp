#include "Common/TestUtilities.h"
#include "Core/Log.h"
#include "Level/Level.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/Light.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/Renderer.h"
#include "Rendering/TextureCapture.h"
#include "Runtime/RenderContext.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

using namespace BrightBox3D;
using namespace BrightBox3D::Geometry;
using namespace BrightBox3D::Rendering;

namespace
{
    Level::LevelObject* AddMesh(Level::Level& level, const Core::Guid& model, const Math::Vector3& position, const Math::Vector3& scale)
    {
        Level::LevelObject* object = *level.CreateObject(Level::LevelObject::StaticType());
        object->GetTransform().SetLocalPosition(position);
        object->GetTransform().SetLocalScale(scale);
        auto* renderer = *object->AddComponent<MeshRenderer>();
        renderer->SetModel(Assets::AssetReference<ModelAsset>(model));
        return object;
    }
}

TEST(RendererTests, RendersLitSceneWithShadows)
{
    std::unique_ptr<Runtime::RenderContext> context = Tests::CreateTestRenderContext();
    ASSERT_NE(context, nullptr);
    RHI::Device& device = context->GetDevice();
    Renderer& renderer = context->GetRenderer();

    Level::Level level;
    AddMesh(level, BuiltinAssets::Plane(), { 0, 0, 0 }, { 2, 1, 2 });
    AddMesh(level, BuiltinAssets::Cube(), { -1.5f, 0.5f, 0 }, { 1, 1, 1 });
    AddMesh(level, BuiltinAssets::Sphere(), { 1.0f, 0.75f, 0.5f }, { 1.5f, 1.5f, 1.5f });
    AddMesh(level, BuiltinAssets::Capsule(), { 0.0f, 1.0f, 2.5f }, { 1, 1, 1 });

    Level::LevelObject* sun = *level.CreateObject(Level::LevelObject::StaticType());
    sun->GetTransform().SetLocalEulerAngles({ 50.0f, -30.0f, 0.0f });
    auto* sunLight = *sun->AddComponent<Light>();
    sunLight->SetLightType(LightType::Directional);
    sunLight->SetIntensity(3.0f);

    Level::LevelObject* lamp = *level.CreateObject(Level::LevelObject::StaticType());
    lamp->GetTransform().SetLocalPosition({ 2.5f, 1.5f, -1.5f });
    auto* lampLight = *lamp->AddComponent<Light>();
    lampLight->SetLightType(LightType::Point);
    lampLight->SetColor({ 1.0f, 0.4f, 0.1f, 1.0f });
    lampLight->SetIntensity(8.0f);
    lampLight->SetRange(6.0f);

    RenderView view;
    view.id = 1;
    view.width = 640;
    view.height = 360;
    view.position = { 0.0f, 3.0f, -7.0f };
    view.view = Math::Matrix4::LookAtLH(view.position, { 0, 0.5f, 0 }, Math::Vector3::Up());
    view.projection = Math::Matrix4::PerspectiveFovLH(view.verticalFov * Math::DegreesToRadians, 640.0f / 360.0f, view.nearPlane, view.farPlane);
    view.showGrid = false;

    RHI::TextureDesc outputDesc;
    outputDesc.width = 640;
    outputDesc.height = 360;
    outputDesc.format = RHI::Format::RGBA8Unorm;
    outputDesc.usage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
    auto output = device.CreateTexture(outputDesc);

    for (int frame = 0; frame < 2; ++frame)
    {
        RHI::CommandList& commands = device.BeginFrame();
        renderer.Render(commands, &level, view, output.get());
        device.EndFrame({});
    }

    EXPECT_GT(renderer.GetStatistics().drawCalls, 4u);
    EXPECT_EQ(renderer.GetStatistics().lights, 2u);
    EXPECT_GT(renderer.GetStatistics().shadowCasters, 0u);

    std::vector<uint8_t> pixels;
    if (std::getenv("BRIGHT_NO_SHADOWS") || std::getenv("BRIGHT_DEBUG_MODE"))
    {
        view.shadows.enabled = std::getenv("BRIGHT_NO_SHADOWS") == nullptr;
        if (const char* mode = std::getenv("BRIGHT_DEBUG_MODE"))
        {
            view.debugMode = static_cast<RenderDebugMode>(std::atoi(mode));
        }
        RHI::CommandList& commands = device.BeginFrame();
        renderer.Render(commands, &level, view, output.get());
        device.EndFrame({});
    }
    ASSERT_TRUE(Rendering::ReadbackRGBA(device, *output, pixels));
    Rendering::SavePng(device, *output, Tests::TestOutputPath(std::getenv("BRIGHT_DEBUG_MODE") ? "BrightBox3D_RendererTestDebug.png" : (std::getenv("BRIGHT_NO_SHADOWS") ? "BrightBox3D_RendererTestNoShadows.png" : "BrightBox3D_RendererTest.png")));

    auto pixel = [&](int x, int y)
    {
        size_t index = (static_cast<size_t>(y) * 640 + x) * 4;
        return std::array<int, 3>{ pixels[index], pixels[index + 1], pixels[index + 2] };
    };
    auto sky = pixel(320, 5);
    EXPECT_GT(sky[2], sky[0]);
    auto ground = pixel(320, 330);
    EXPECT_GT(ground[0] + ground[1] + ground[2], 60);
}

TEST(RendererTests, PostProcessingAndAntiAliasingModes)
{
    std::unique_ptr<Runtime::RenderContext> context = Tests::CreateTestRenderContext();
    ASSERT_NE(context, nullptr);
    RHI::Device& device = context->GetDevice();
    Renderer& renderer = context->GetRenderer();

    Level::Level level;
    AddMesh(level, BuiltinAssets::Plane(), { 0, 0, 0 }, { 2, 1, 2 });
    for (int i = 0; i < 5; ++i)
    {
        AddMesh(level, BuiltinAssets::Cube(), { -3.0f + i * 1.5f, 0.5f, static_cast<float>(i % 2) }, { 1, 1 + i * 0.3f, 1 });
    }
    Level::LevelObject* glow = AddMesh(level, BuiltinAssets::Sphere(), { 0.0f, 2.2f, 1.5f }, { 0.8f, 0.8f, 0.8f });
    (void)glow;
    Level::LevelObject* sun = *level.CreateObject(Level::LevelObject::StaticType());
    sun->GetTransform().SetLocalEulerAngles({ 40.0f, 30.0f, 0.0f });
    auto* light = *sun->AddComponent<Light>();
    light->SetLightType(LightType::Directional);
    light->SetIntensity(3.0f);
    Level::LevelObject* lamp = *level.CreateObject(Level::LevelObject::StaticType());
    lamp->GetTransform().SetLocalPosition({ 0.0f, 2.2f, 0.4f });
    auto* lampLight = *lamp->AddComponent<Light>();
    lampLight->SetColor({ 0.3f, 0.6f, 1.0f, 1.0f });
    lampLight->SetIntensity(60.0f);
    lampLight->SetRange(5.0f);

    RHI::TextureDesc outputDesc;
    outputDesc.width = 480;
    outputDesc.height = 270;
    outputDesc.format = RHI::Format::RGBA8Unorm;
    outputDesc.usage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
    auto output = device.CreateTexture(outputDesc);

    int errorsBefore = Core::Log::Count(Core::LogLevel::Error, Core::LogSource::Engine);
    const char* names[] = { "None", "FXAA", "SMAA", "TAA" };
    for (int mode = 0; mode < 4; ++mode)
    {
        RenderView view;
        view.id = 100 + static_cast<uint64_t>(mode);
        view.width = 480;
        view.height = 270;
        view.position = { 0.0f, 3.0f, -6.0f };
        view.view = Math::Matrix4::LookAtLH(view.position, { 0, 0.8f, 0 }, Math::Vector3::Up());
        view.projection = Math::Matrix4::PerspectiveFovLH(view.verticalFov * Math::DegreesToRadians, 480.0f / 270.0f, view.nearPlane, view.farPlane);
        view.post.antiAliasing = static_cast<AntiAliasing>(mode);
        view.post.autoExposure = mode == 3;
        view.post.bloomIntensity = 0.08f;
        view.post.ssao = true;
        view.post.vignetteIntensity = 0.3f;
        for (int frame = 0; frame < (mode == 3 ? 12 : 1); ++frame)
        {
            RHI::CommandList& commands = device.BeginFrame();
            renderer.Render(commands, &level, view, output.get());
            device.EndFrame({});
        }
        std::vector<uint8_t> pixels;
        ASSERT_TRUE(Rendering::ReadbackRGBA(device, *output, pixels));
        Rendering::SavePng(device, *output, Tests::TestOutputPath(std::string("BrightBox3D_AA_") + names[mode] + ".png"));
        uint64_t sum = 0;
        for (size_t i = 0; i < pixels.size(); i += 4)
        {
            sum += pixels[i] + pixels[i + 1] + pixels[i + 2];
        }
        EXPECT_GT(sum / (pixels.size() / 4), 60u) << names[mode];
    }
    EXPECT_EQ(Core::Log::Count(Core::LogLevel::Error, Core::LogSource::Engine), errorsBefore);
}
