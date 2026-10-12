#pragma once

#include "Core/FileWatcher.h"
#include "EditorContext.h"
#include "Platform/Window.h"
#include "RHI/Device.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/Renderer.h"
#include "Rendering/ShaderLibrary.h"

#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Editor
{
    class ImGuiPlatform;
    class ImGuiRenderer;
    class LevelViewPanel;
    class LevelContentsPanel;
    class InspectorPanel;
    class AssetsPanel;
    class TerminalPanel;
    class ProjectSettingsPanel;
    class ThumbnailCache;
    class EditorPlaySession;
    class HotReload;

    struct ViewRenderRequest
    {
        Level::Level* level = nullptr;
        Rendering::RenderView view;
        RHI::Texture* target = nullptr;
    };

    class EditorApplication
    {
    public:
        EditorApplication();
        ~EditorApplication();

        bool Initialize(int argc, char** argv);
        void Run();
        void Shutdown();

        EditorContext& GetContext() { return m_context; }
        RHI::Device& GetDevice() { return *m_device; }
        Rendering::Renderer& GetRenderer() { return *m_renderer; }
        Rendering::GpuResourceCache& GetResources() { return *m_resources; }
        Rendering::ShaderLibrary& GetShaders() { return *m_shaders; }
        ThumbnailCache& GetThumbnails() { return *m_thumbnails; }
        Platform::Window& GetWindow() { return *m_window; }
        HotReload* GetHotReload() { return m_hotReload.get(); }
        EditorPlaySession* GetPlaySession() { return m_playSession.get(); }
        LevelViewPanel& GetLevelView() { return *m_levelView; }

        void RequestRender(const ViewRenderRequest& request) { m_renderRequests.push_back(request); }
        void RequestPlay();
        void RequestStop();
        void OpenProjectSettings();
        void OpenFileInEditor(const std::string& path, int line);
        void RevealInFileBrowser(const std::filesystem::path& path);

    private:
        void Frame(float deltaTime);
        void DrawLauncher();
        void DrawMainMenu();
        void DrawToolbar();
        void DrawDockSpace();
        void DrawStatusBar();
        void HandleShortcuts();
        bool OpenProject(const std::filesystem::path& path);
        void LoadRecentProjects();
        void SaveRecentProjects();
        void AddRecentProject(const std::filesystem::path& path);
        void ProcessFileChanges();
        void UpdateTitle();
        std::filesystem::path GetUserDirectory() const;

        std::unique_ptr<Platform::Window> m_window;
        std::unique_ptr<RHI::Device> m_device;
        std::unique_ptr<RHI::Swapchain> m_swapchain;
        std::unique_ptr<Rendering::ShaderLibrary> m_shaders;
        std::unique_ptr<Rendering::GpuResourceCache> m_resources;
        std::unique_ptr<Rendering::Renderer> m_renderer;
        std::unique_ptr<ImGuiPlatform> m_imguiPlatform;
        std::unique_ptr<ImGuiRenderer> m_imguiRenderer;
        std::unique_ptr<ThumbnailCache> m_thumbnails;
        std::unique_ptr<LevelViewPanel> m_levelView;
        std::unique_ptr<LevelContentsPanel> m_levelContents;
        std::unique_ptr<InspectorPanel> m_inspector;
        std::unique_ptr<AssetsPanel> m_assets;
        std::unique_ptr<TerminalPanel> m_terminal;
        std::unique_ptr<ProjectSettingsPanel> m_projectSettings;
        std::unique_ptr<EditorPlaySession> m_playSession;
        std::unique_ptr<HotReload> m_hotReload;
        Core::FileWatcher m_fileWatcher;
        EditorContext m_context;
        std::vector<ViewRenderRequest> m_renderRequests;
        std::vector<std::string> m_recentProjects;
        std::string m_launcherPath;
        std::string m_launcherName = "MyGame";
        std::string m_launcherLocation;
        std::string m_launcherError;
        std::string m_lastTitle;
        bool m_resetLayout = false;
        bool m_running = true;
        bool m_requestPlay = false;
        bool m_requestStop = false;
        std::string m_newLevelName;
        bool m_openNewLevelPopup = false;
        std::string m_iniPath;
    };
}
