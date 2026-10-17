#include "PlayerApplication.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineInfo.h"
#include "Core/EngineText.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Framework/Camera.h"
#include "Framework/CameraScreen.h"
#include "Framework/GameSession.h"
#include "Level/LevelSerializer.h"
#include "Platform/Input.h"
#include "Rendering/DebugDraw.h"
#include "Rendering/TextureCapture.h"
#include "Runtime/BuildLayout.h"
#include "Runtime/FrameClock.h"
#include "Runtime/GameplayMigration.h"
#include "Runtime/LevelObjectAssetMigration.h"
#include "Runtime/Project.h"

#include <string>

namespace BrightBox3D::Player
{
    namespace
    {
        constexpr uint64_t PlayerViewId = 0x504C5952;
        constexpr const char* ShadowModulePrefix = "Player_";
        constexpr int DefaultWindowWidth = 1280;
        constexpr int DefaultWindowHeight = 720;
    }

    bool PlayerApplication::Initialize(const PlayerOptions& options)
    {
        m_options = options;
        bool opened = m_options.project.empty() ? OpenPackagedGame() : OpenProject();
        if (!opened || !CreateWindowAndDevice())
        {
            return false;
        }
        LoadGameModule();
        if (!LoadStartupLevel())
        {
            return false;
        }
        SubscribeInput();
        Framework::GameSession::Start(*m_level);
        return true;
    }

    bool PlayerApplication::OpenProject()
    {
        Runtime::Project project;
        if (Core::Result<void> opened = project.Open(m_options.project); !opened)
        {
            Core::Log::Error(Core::Text(Core::EngineText::PlayerCannotOpenProject, opened.GetError().GetText()));
            return false;
        }
        m_manifest.AssignFromProject(project);
        m_root = project.GetRoot();
        m_modulePath = project.GetGameModulePath();
        Runtime::MigrateLegacyLevelObjectAssets(project);
        Assets::AssetDatabase::Get().Open(m_root, Assets::AssetDatabaseMode::Editor);
        Runtime::MigrateLegacyGameplaySettings(project);
        return true;
    }

    bool PlayerApplication::OpenPackagedGame()
    {
        m_root = Core::FileSystem::ExecutableDirectory();
        if (Core::Result<void> loaded = m_manifest.Load(Runtime::BuildLayout::GetManifestPath(m_root)); !loaded)
        {
            Core::Log::Error(Core::Text(Core::EngineText::PlayerCannotReadManifest, Runtime::GameManifest::FileName, loaded.GetError().GetText()));
            return false;
        }
        m_modulePath = Runtime::BuildLayout::GetGameModulePath(m_root, m_manifest.gameModule);
        Assets::AssetDatabase::Get().Open(m_root, Assets::AssetDatabaseMode::Cooked);
        return true;
    }

    bool PlayerApplication::CreateWindowAndDevice()
    {
        Platform::WindowDesc desc;
        desc.title = m_manifest.name;
        desc.width = DefaultWindowWidth;
        desc.height = DefaultWindowHeight;
        m_window = std::make_unique<Platform::Window>(desc);
        if (!m_window->IsValid())
        {
            return false;
        }
        m_renderContext = Runtime::RenderContext::Create(m_window.get(), Runtime::RenderContext::DefaultShaderDirectory());
        return m_renderContext != nullptr;
    }

    void PlayerApplication::LoadGameModule()
    {
        if (m_manifest.gameModule.empty() || !std::filesystem::exists(m_modulePath))
        {
            return;
        }
        std::filesystem::path shadow = std::filesystem::temp_directory_path() / Core::EngineName / (ShadowModulePrefix + m_manifest.gameModule);
        if (Core::Result<void> loaded = m_module.Load(m_modulePath, m_manifest.gameModule, shadow); !loaded)
        {
            Core::Log::Error(Core::Text(Core::EngineText::PlayerModuleLoadFailed, loaded.GetError().GetText()));
        }
    }

    bool PlayerApplication::LoadStartupLevel()
    {
        Core::Result<std::unique_ptr<Level::Level>> level = Level::LevelSerializer::LoadLevel(m_manifest.startupLevel);
        if (!level)
        {
            Core::Log::Error(Core::Text(Core::EngineText::PlayerStartupLevelFailed, level.GetError().GetText()));
            return false;
        }
        m_level = std::move(*level);
        m_level->SetFixedDeltaTime(m_manifest.fixedTimeStep);
        return true;
    }

    void PlayerApplication::SubscribeInput()
    {
        Platform::Window& window = *m_window;
        Platform::Input::SetWindow(&window);
        window.OnKey.Subscribe([&window](const Platform::KeyEvent& event)
        {
            if (event.down && event.key == Platform::Key::Escape && window.GetCursorMode() == Platform::CursorMode::Locked)
            {
                window.SetCursorMode(Platform::CursorMode::Normal);
            }
        });
    }

    void PlayerApplication::RenderFrame(RHI::CommandList& commands, RHI::Texture* backbuffer)
    {
        if (Framework::Camera* camera = Framework::Camera::FindActive(*m_level))
        {
            m_renderContext->GetRenderer().Render(commands, m_level.get(), camera->BuildRenderView(backbuffer->GetWidth(), backbuffer->GetHeight(), PlayerViewId), backbuffer);
            Framework::RenderCameraScreen(commands, *camera, *m_level, backbuffer, m_window.get());
            return;
        }
        RHI::RenderPassDesc pass;
        pass.colors.push_back({ backbuffer, 0, 0, RHI::LoadAction::Clear, { 0.0f, 0.0f, 0.0f, 1.0f } });
        commands.BeginRenderPass(pass);
        commands.EndRenderPass();
    }

    void PlayerApplication::SaveScreenshot(RHI::CommandList& commands, RHI::Texture* backbuffer)
    {
        RHI::Device& device = m_renderContext->GetDevice();
        RHI::TextureDesc copyDesc;
        copyDesc.width = backbuffer->GetWidth();
        copyDesc.height = backbuffer->GetHeight();
        copyDesc.format = backbuffer->GetFormat();
        copyDesc.usage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
        auto copy = device.CreateTexture(copyDesc);
        commands.CopyTexture(backbuffer, copy.get());
        device.EndFrame({ m_renderContext->GetSwapchain() });
        if (Rendering::SavePng(device, *copy, m_options.screenshot))
        {
            Core::Log::Info(Core::Text(Core::EngineText::SavedScreenshot, m_options.screenshot.string()));
        }
    }

    void PlayerApplication::Run()
    {
        RHI::Device& device = m_renderContext->GetDevice();
        RHI::Swapchain& swapchain = *m_renderContext->GetSwapchain();
        Runtime::FrameClock clock;
        int frame = 0;
        while (!m_window->ShouldClose())
        {
            Platform::Window::PollEvents();
            float deltaTime = clock.Tick();
            m_window->GetInput().BeginFrame();
            m_level->Update(deltaTime);
            if (m_window->IsMinimized())
            {
                Runtime::FrameClock::WaitWhileMinimized();
                continue;
            }
            m_renderContext->GetResources().ProcessInvalidations();
            RHI::CommandList& commands = device.BeginFrame();
            RHI::Texture* backbuffer = swapchain.AcquireNextTexture();
            if (backbuffer)
            {
                RenderFrame(commands, backbuffer);
            }
            ++frame;
            bool capture = m_options.frames > 0 && frame >= m_options.frames;
            if (capture && backbuffer && !m_options.screenshot.empty())
            {
                SaveScreenshot(commands, backbuffer);
                break;
            }
            device.EndFrame({ &swapchain });
            Rendering::DebugDraw::Clear();
            if (capture)
            {
                break;
            }
        }
    }

    void PlayerApplication::Shutdown()
    {
        m_renderContext->GetDevice().WaitIdle();
        m_level->EndPlay();
        m_renderContext.reset();
        m_level.reset();
        Level::LevelObjectLibrary::Get().Clear();
        m_module.Unload();
        Platform::Input::SetWindow(nullptr);
        Assets::AssetDatabase::Get().Close();
    }
}
