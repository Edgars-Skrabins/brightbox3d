#include "Assets/AssetDatabase.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/SharedLibrary.h"
#include "Framework/Camera.h"
#include "Framework/GameSession.h"
#include "Level/LevelSerializer.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "RHI/Device.h"
#include "Rendering/DebugDraw.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/Renderer.h"
#include "Rendering/ShaderLibrary.h"
#include "Runtime/GameManifest.h"
#include "Runtime/GameModule.h"
#include "Runtime/Project.h"
#include "UI/ScreenRenderer.h"

#include <stb_image_write.h>

#include <cstdlib>
#include <string>

using namespace BrightBox3D;

namespace
{
    constexpr uint64_t PlayerViewId = 0x504C5952;

    void SaveScreenshot(RHI::Device& device, RHI::Texture* texture, const std::string& path)
    {
        std::vector<uint8_t> pixels;
        if (!texture || !device.ReadTexture(texture, pixels))
        {
            return;
        }
        bool bgra = texture->GetFormat() == RHI::Format::BGRA8Unorm;
        for (size_t i = 0; i + 3 < pixels.size(); i += 4)
        {
            if (bgra)
            {
                std::swap(pixels[i], pixels[i + 2]);
            }
            pixels[i + 3] = 255;
        }
        stbi_write_png(path.c_str(), static_cast<int>(texture->GetWidth()), static_cast<int>(texture->GetHeight()), 4, pixels.data(), static_cast<int>(texture->GetWidth()) * 4);
        Core::Log::Info("Saved screenshot '" + path + "'");
    }
}

int main(int argc, char** argv)
{
    std::filesystem::path projectPath;
    std::string screenshotPath;
    int exitAfterFrames = -1;
    for (int i = 1; i < argc; ++i)
    {
        std::string argument = argv[i];
        if (argument == "--project" && i + 1 < argc)
        {
            projectPath = argv[++i];
        }
        else if (argument == "--frames" && i + 1 < argc)
        {
            exitAfterFrames = std::atoi(argv[++i]);
        }
        else if (argument == "--screenshot" && i + 1 < argc)
        {
            screenshotPath = argv[++i];
        }
    }

    Runtime::GameManifest manifest;
    std::filesystem::path root;
    std::filesystem::path modulePath;
    std::string error;
    if (!projectPath.empty())
    {
        Runtime::Project project;
        if (!project.Open(projectPath, error))
        {
            Core::Log::Error("Cannot open project: " + error);
            return 1;
        }
        manifest.name = project.GetName();
        manifest.gameModule = project.GetGameModuleName();
        manifest.startupLevel = project.GetStartupLevel();
        manifest.fixedTimeStep = project.GetFixedTimeStep();
        manifest.gameMode.CopyFrom(project.GetGameMode(), manifest.gameMode.GetAllocator());
        root = project.GetRoot();
        modulePath = root / "Binaries" / Core::SharedLibrary::PlatformFileName(manifest.gameModule);
        Assets::AssetDatabase::Get().Open(root, Assets::AssetDatabaseMode::Editor);
    }
    else
    {
        root = Core::FileSystem::ExecutableDirectory();
        if (!manifest.Load(root / Runtime::GameManifest::FileName, error))
        {
            Core::Log::Error("Cannot read " + std::string(Runtime::GameManifest::FileName) + ": " + error);
            return 1;
        }
        modulePath = root / Core::SharedLibrary::PlatformFileName(manifest.gameModule);
        Assets::AssetDatabase::Get().Open(root, Assets::AssetDatabaseMode::Cooked);
    }

    Platform::WindowDesc desc;
    desc.title = manifest.name;
    desc.width = 1280;
    desc.height = 720;
    Platform::Window window(desc);
    if (!window.IsValid())
    {
        return 1;
    }
    std::unique_ptr<RHI::Device> device = RHI::Device::Create();
    if (!device)
    {
        return 1;
    }
    std::unique_ptr<RHI::Swapchain> swapchain = device->CreateSwapchain(window, true);
    Rendering::ShaderLibrary shaders(*device, Core::FileSystem::EngineContentDirectory() / "Shaders");
    Rendering::GpuResourceCache resources(*device);
    Rendering::Renderer renderer(*device, shaders, resources);
    UI::ScreenRenderer::Get().Initialize(*device, shaders, resources);

    Runtime::GameModule module;
    if (!manifest.gameModule.empty() && std::filesystem::exists(modulePath))
    {
        std::filesystem::path shadow = std::filesystem::temp_directory_path() / "BrightBox3D" / ("Player_" + manifest.gameModule);
        if (!module.Load(modulePath, manifest.gameModule, shadow, error))
        {
            Core::Log::Error("Game module failed to load: " + error);
        }
    }

    std::unique_ptr<Level::Level> level = Level::LevelSerializer::LoadLevel(manifest.startupLevel, &error);
    if (!level)
    {
        Core::Log::Error("Startup level could not be loaded: " + error);
        return 1;
    }
    level->SetFixedDeltaTime(manifest.fixedTimeStep);
    Platform::Input::SetWindow(&window);
    window.OnKey.Subscribe([&window](const Platform::KeyEvent& event)
    {
        if (event.down && event.key == Platform::Key::Escape && window.GetCursorMode() == Platform::CursorMode::Locked)
        {
            window.SetCursorMode(Platform::CursorMode::Normal);
        }
    });
    Framework::GameSession::Start(*level, manifest.gameMode);

    double previous = Platform::Window::GetTime();
    int frame = 0;
    while (!window.ShouldClose())
    {
        Platform::Window::PollEvents();
        double now = Platform::Window::GetTime();
        float deltaTime = static_cast<float>(std::min(now - previous, 0.1));
        previous = now;
        window.GetInput().BeginFrame();
        level->Update(deltaTime);
        if (window.IsMinimized())
        {
            Platform::Window::WaitEvents(0.05);
            continue;
        }
        resources.ProcessInvalidations();
        RHI::CommandList& commands = device->BeginFrame();
        RHI::Texture* backbuffer = swapchain->AcquireNextTexture();
        if (backbuffer)
        {
            Framework::Camera* camera = Framework::Camera::FindActive(*level);
            if (camera)
            {
                renderer.Render(commands, level.get(), camera->BuildRenderView(backbuffer->GetWidth(), backbuffer->GetHeight(), PlayerViewId), backbuffer);
                UI::ScreenRenderer::Get().RenderCameraScreen(commands, *camera, *level, backbuffer, &window);
            }
            else
            {
                RHI::RenderPassDesc pass;
                pass.colors.push_back({ backbuffer, 0, 0, RHI::LoadAction::Clear, { 0.0f, 0.0f, 0.0f, 1.0f } });
                commands.BeginRenderPass(pass);
                commands.EndRenderPass();
            }
        }
        ++frame;
        bool capture = exitAfterFrames > 0 && frame >= exitAfterFrames;
        if (capture && backbuffer && !screenshotPath.empty())
        {
            RHI::TextureDesc copyDesc;
            copyDesc.width = backbuffer->GetWidth();
            copyDesc.height = backbuffer->GetHeight();
            copyDesc.format = backbuffer->GetFormat();
            copyDesc.usage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
            auto copy = device->CreateTexture(copyDesc);
            commands.CopyTexture(backbuffer, copy.get());
            device->EndFrame({ swapchain.get() });
            SaveScreenshot(*device, copy.get(), screenshotPath);
            break;
        }
        device->EndFrame({ swapchain.get() });
        Rendering::DebugDraw::Clear();
        if (capture)
        {
            break;
        }
    }

    device->WaitIdle();
    level->EndPlay();
    UI::ScreenRenderer::Get().Shutdown();
    level.reset();
    Level::LevelObjectLibrary::Get().Clear();
    module.Unload();
    Platform::Input::SetWindow(nullptr);
    Assets::AssetDatabase::Get().Close();
    return 0;
}
