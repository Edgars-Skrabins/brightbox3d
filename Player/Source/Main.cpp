#include "Assets/AssetDatabase.h"

#include "Core/CommandLine.h"
#include "Core/EngineInfo.h"
#include "Core/EngineText.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Framework/Camera.h"
#include "Framework/CameraScreen.h"
#include "Framework/GameSession.h"
#include "Level/LevelSerializer.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "PlayerCommandLine.h"
#include "Rendering/DebugDraw.h"
#include "Rendering/TextureCapture.h"
#include "Runtime/BuildLayout.h"
#include "Runtime/FrameClock.h"
#include "Runtime/GameManifest.h"
#include "Runtime/GameModule.h"
#include "Runtime/GameplayMigration.h"
#include "Runtime/LevelObjectAssetMigration.h"
#include "Runtime/Project.h"
#include "Runtime/RenderContext.h"

#include <optional>
#include <string>

using namespace BrightBox3D;

namespace
{
    constexpr uint64_t PlayerViewId = 0x504C5952;
    constexpr const char* ShadowModulePrefix = "Player_";
}

int main(int argc, char** argv)
{
    std::string commandLineError;
    std::optional<Player::PlayerOptions> options = Core::ParseCommandLine<Player::PlayerOptions>(argc, argv, Player::PlayerCommandLine, commandLineError);
    if (!options)
    {
        Core::Log::Error(commandLineError);
        Core::Log::Info(Core::CommandLineUsage<Player::PlayerOptions>(std::filesystem::path(argv[0]).filename().string(), Player::PlayerCommandLine));
        return 1;
    }
    const std::filesystem::path& projectPath = options->project;
    const std::filesystem::path& screenshotPath = options->screenshot;
    int exitAfterFrames = options->frames;

    Runtime::GameManifest manifest;
    std::filesystem::path root;
    std::filesystem::path modulePath;
    std::string error;
    if (!projectPath.empty())
    {
        Runtime::Project project;
        if (!project.Open(projectPath, error))
        {
            Core::Log::Error(Core::Text(Core::EngineText::PlayerCannotOpenProject, error));
            return 1;
        }
        manifest.AssignFromProject(project);
        root = project.GetRoot();
        modulePath = project.GetGameModulePath();
        Runtime::MigrateLegacyLevelObjectAssets(project);
        Assets::AssetDatabase::Get().Open(root, Assets::AssetDatabaseMode::Editor);
        Runtime::MigrateLegacyGameplaySettings(project);
    }
    else
    {
        root = Core::FileSystem::ExecutableDirectory();
        if (!manifest.Load(Runtime::BuildLayout::GetManifestPath(root), error))
        {
            Core::Log::Error(Core::Text(Core::EngineText::PlayerCannotReadManifest, Runtime::GameManifest::FileName, error));
            return 1;
        }
        modulePath = Runtime::BuildLayout::GetGameModulePath(root, manifest.gameModule);
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
    std::unique_ptr<Runtime::RenderContext> renderContext = Runtime::RenderContext::Create(&window, Runtime::RenderContext::DefaultShaderDirectory());
    if (!renderContext)
    {
        return 1;
    }
    RHI::Device& device = renderContext->GetDevice();
    RHI::Swapchain& swapchain = *renderContext->GetSwapchain();

    Runtime::GameModule module;
    if (!manifest.gameModule.empty() && std::filesystem::exists(modulePath))
    {
        std::filesystem::path shadow = std::filesystem::temp_directory_path() / Core::EngineName / (ShadowModulePrefix + manifest.gameModule);
        if (!module.Load(modulePath, manifest.gameModule, shadow, error))
        {
            Core::Log::Error(Core::Text(Core::EngineText::PlayerModuleLoadFailed, error));
        }
    }

    std::unique_ptr<Level::Level> level = Level::LevelSerializer::LoadLevel(manifest.startupLevel, &error);
    if (!level)
    {
        Core::Log::Error(Core::Text(Core::EngineText::PlayerStartupLevelFailed, error));
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
    Framework::GameSession::Start(*level);

    Runtime::FrameClock clock;
    int frame = 0;
    while (!window.ShouldClose())
    {
        Platform::Window::PollEvents();
        float deltaTime = clock.Tick();
        window.GetInput().BeginFrame();
        level->Update(deltaTime);
        if (window.IsMinimized())
        {
            Runtime::FrameClock::WaitWhileMinimized();
            continue;
        }
        renderContext->GetResources().ProcessInvalidations();
        RHI::CommandList& commands = device.BeginFrame();
        RHI::Texture* backbuffer = swapchain.AcquireNextTexture();
        if (backbuffer)
        {
            Framework::Camera* camera = Framework::Camera::FindActive(*level);
            if (camera)
            {
                renderContext->GetRenderer().Render(commands, level.get(), camera->BuildRenderView(backbuffer->GetWidth(), backbuffer->GetHeight(), PlayerViewId), backbuffer);
                Framework::RenderCameraScreen(commands, *camera, *level, backbuffer, &window);
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
            auto copy = device.CreateTexture(copyDesc);
            commands.CopyTexture(backbuffer, copy.get());
            device.EndFrame({ &swapchain });
            if (Rendering::SavePng(device, *copy, screenshotPath))
            {
                Core::Log::Info(Core::Text(Core::EngineText::SavedScreenshot, screenshotPath.string()));
            }
            break;
        }
        device.EndFrame({ &swapchain });
        Rendering::DebugDraw::Clear();
        if (capture)
        {
            break;
        }
    }

    device.WaitIdle();
    level->EndPlay();
    renderContext.reset();
    level.reset();
    Level::LevelObjectLibrary::Get().Clear();
    module.Unload();
    Platform::Input::SetWindow(nullptr);
    Assets::AssetDatabase::Get().Close();
    return 0;
}
