#include "Level/Level.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/Light.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/Renderer.h"

#include <gtest/gtest.h>

#include <stb_image_write.h>

#include <cstdlib>
#include <filesystem>

using namespace BrightBox3D;
using namespace BrightBox3D::Rendering;

namespace
{
    Level::LevelObjectInstance* AddMesh(Level::Level& level, const Core::Guid& model, const Math::Vector3& position, const Math::Vector3& scale)
    {
        Level::LevelObjectInstance* object = level.CreateObject(Level::LevelObjectInstance::StaticType());
        object->GetTransform().SetLocalPosition(position);
        object->GetTransform().SetLocalScale(scale);
        auto* renderer = object->AddComponent<MeshRenderer>();
        renderer->SetModel(Assets::AssetReference<ModelAsset>(model));
        return object;
    }

    std::filesystem::path OutputDirectory()
    {
        const char* directory = std::getenv("BRIGHT_TEST_OUTPUT");
        return directory ? std::filesystem::path(directory) : std::filesystem::temp_directory_path();
    }
}

TEST(RendererTests, RendersLitSceneWithShadows)
{
    std::unique_ptr<RHI::Device> device = RHI::Device::Create();
    ASSERT_NE(device, nullptr);
    ShaderLibrary shaders(*device, BRIGHT_ENGINE_CONTENT_DIR "/Shaders");
    GpuResourceCache resources(*device);
    Renderer renderer(*device, shaders, resources);

    Level::Level level;
    AddMesh(level, BuiltinAssets::Plane(), { 0, 0, 0 }, { 2, 1, 2 });
    AddMesh(level, BuiltinAssets::Cube(), { -1.5f, 0.5f, 0 }, { 1, 1, 1 });
    AddMesh(level, BuiltinAssets::Sphere(), { 1.0f, 0.75f, 0.5f }, { 1.5f, 1.5f, 1.5f });
    AddMesh(level, BuiltinAssets::Capsule(), { 0.0f, 1.0f, 2.5f }, { 1, 1, 1 });

    Level::LevelObjectInstance* sun = level.CreateObject(Level::LevelObjectInstance::StaticType());
    sun->GetTransform().SetLocalEulerAngles({ 50.0f, -30.0f, 0.0f });
    auto* sunLight = sun->AddComponent<Light>();
    sunLight->SetLightType(LightType::Directional);
    sunLight->SetIntensity(3.0f);

    Level::LevelObjectInstance* lamp = level.CreateObject(Level::LevelObjectInstance::StaticType());
    lamp->GetTransform().SetLocalPosition({ 2.5f, 1.5f, -1.5f });
    auto* lampLight = lamp->AddComponent<Light>();
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
    auto output = device->CreateTexture(outputDesc);

    for (int frame = 0; frame < 2; ++frame)
    {
        RHI::CommandList& commands = device->BeginFrame();
        renderer.Render(commands, &level, view, output.get());
        device->EndFrame({});
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
        RHI::CommandList& commands = device->BeginFrame();
        renderer.Render(commands, &level, view, output.get());
        device->EndFrame({});
    }
    ASSERT_TRUE(device->ReadTexture(output.get(), pixels));
    std::filesystem::path path = OutputDirectory() / (std::getenv("BRIGHT_DEBUG_MODE") ? "BrightBox3D_RendererTestDebug.png" : (std::getenv("BRIGHT_NO_SHADOWS") ? "BrightBox3D_RendererTestNoShadows.png" : "BrightBox3D_RendererTest.png"));
    stbi_write_png(path.string().c_str(), 640, 360, 4, pixels.data(), 640 * 4);

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
