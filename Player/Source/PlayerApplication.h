#pragma once

#include "Level/Level.h"
#include "Platform/Window.h"
#include "PlayerCommandLine.h"
#include "Runtime/GameManifest.h"
#include "Runtime/GameModule.h"
#include "Runtime/RenderContext.h"

#include <filesystem>
#include <memory>

namespace BrightBox3D::Player
{
    class PlayerApplication
    {
    public:
        bool Initialize(const PlayerOptions& options);
        void Run();
        void Shutdown();

    private:
        bool OpenProject();
        bool OpenPackagedGame();
        bool CreateWindowAndDevice();
        void LoadGameModule();
        bool LoadStartupLevel();
        void SubscribeInput();
        void RenderFrame(RHI::CommandList& commands, RHI::Texture* backbuffer);
        void SaveScreenshot(RHI::CommandList& commands, RHI::Texture* backbuffer);

        PlayerOptions m_options;
        Runtime::GameManifest m_manifest;
        std::filesystem::path m_root;
        std::filesystem::path m_modulePath;
        std::unique_ptr<Platform::Window> m_window;
        std::unique_ptr<Runtime::RenderContext> m_renderContext;
        Runtime::GameModule m_module;
        std::unique_ptr<Level::Level> m_level;
    };
}
