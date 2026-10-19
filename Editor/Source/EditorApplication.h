#pragma once

#include "EditorCommandLine.h"
#include "EditorContext.h"
#include "FileChangeRouter.h"
#include "CreateScriptDialog.h"
#include "NewLevelDialog.h"
#include "Scripts/PendingScriptComponent.h"
#include "Panels/EditorPanel.h"
#include "Platform/Window.h"
#include "ProjectLauncher.h"
#include "RecentProjects.h"
#include "Runtime/RenderContext.h"

#include <array>
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
    class PreferencesPanel;
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
        RHI::Device& GetDevice() { return m_renderContext->GetDevice(); }
        RHI::Swapchain& GetSwapchain() { return *m_renderContext->GetSwapchain(); }
        Rendering::Renderer& GetRenderer() { return m_renderContext->GetRenderer(); }
        Rendering::GpuResourceCache& GetResources() { return m_renderContext->GetResources(); }
        Rendering::ShaderLibrary& GetShaders() { return m_renderContext->GetShaders(); }
        ThumbnailCache& GetThumbnails() { return *m_thumbnails; }
        Platform::Window& GetWindow() { return *m_window; }
        HotReload* GetHotReload() { return m_hotReload.get(); }
        EditorPlaySession* GetPlaySession() { return m_playSession.get(); }
        LevelViewPanel* GetActiveLevelView();
        LevelContentsPanel* GetLevelContents();
        AssetsPanel* GetAssets();
        Math::Vector3 GetSpawnPoint();
        void SetActivePanel(const EditorPanel& panel);
        EditorPanel& CreatePanel(EditorWindow type);
        void AddPanelToDock(EditorWindow type, ImGuiID node);

        void RequestRender(const ViewRenderRequest& request) { m_renderRequests.push_back(request); }
        void RequestPlay();
        void RequestStop();
        void OpenProjectSettings();
        void OpenPreferences();
        void OpenNewLevelDialog();
        void OpenCreateScriptDialog(const CreateScriptRequest& request);
        CreateScriptRequest DefaultScriptRequest();
        PendingScriptComponent& GetPendingScriptComponent() { return m_pendingScriptComponent; }
        void BuildGame();
        void CloseProject();
        void RequestExit();
        void ResetLayout();
        void OpenFileInEditor(const std::string& path, int line);
        void RevealInFileBrowser(const std::filesystem::path& path);

    private:
        bool CreateWindowAndDevice();
        void InitializeImGui();
        void CreatePanels();
        void AddCommandLinePanel();
        void LoadPanels();
        void SavePanels() const;
        EditorPanel& CreatePanelWithIndex(EditorWindow type, int index);
        int NextInstanceIndex(EditorWindow type) const;

        template <typename T>
        T* FindPanel(EditorWindow type) const
        {
            for (const std::unique_ptr<EditorPanel>& panel : m_panels)
            {
                if (panel->GetWindowType() == type && panel->IsOpen())
                {
                    return static_cast<T*>(panel.get());
                }
            }
            return nullptr;
        }

        template <typename T>
        T* FindActivePanel(EditorWindow type) const
        {
            int active = m_activePanels[static_cast<size_t>(type)];
            for (const std::unique_ptr<EditorPanel>& panel : m_panels)
            {
                if (panel->GetWindowType() == type && panel->IsOpen() && panel->GetInstanceIndex() == active)
                {
                    return static_cast<T*>(panel.get());
                }
            }
            return FindPanel<T>(type);
        }
        void SubscribeWindowEvents();
        void ApplyCommandLine();
        void Frame(float deltaTime);
        void DrawMainMenu();
        void DrawToolbar();
        void DrawDockSpace();
        void DrawEditor();
        void UpdatePlayMode(float deltaTime);
        void RenderFrame();
        bool OpenProject(const std::filesystem::path& path);
        void AddRecentProject(const std::filesystem::path& path);
        void UpdateTitle();
        void SaveScreenshot(RHI::CommandList& commands);
        std::filesystem::path GetUserDirectory() const;

        std::unique_ptr<Platform::Window> m_window;
        std::unique_ptr<Runtime::RenderContext> m_renderContext;
        std::unique_ptr<ImGuiPlatform> m_imguiPlatform;
        std::unique_ptr<ImGuiRenderer> m_imguiRenderer;
        std::unique_ptr<ThumbnailCache> m_thumbnails;
        std::vector<std::unique_ptr<EditorPanel>> m_panels;
        std::array<int, static_cast<size_t>(EditorWindow::Count)> m_activePanels{};
        std::unique_ptr<ProjectSettingsPanel> m_projectSettings;
        std::unique_ptr<PreferencesPanel> m_preferences;
        std::unique_ptr<EditorPlaySession> m_playSession;
        std::unique_ptr<HotReload> m_hotReload;
        FileChangeRouter m_fileChanges;
        EditorContext m_context;
        std::vector<ViewRenderRequest> m_renderRequests;
        RecentProjects m_recentProjects;
        ProjectLauncher m_launcher{ m_recentProjects };
        NewLevelDialog m_newLevelDialog;
        CreateScriptDialog m_createScriptDialog;
        PendingScriptComponent m_pendingScriptComponent;
        std::string m_lastTitle;
        bool m_resetLayout = false;
        uint32_t m_themeRevision = 0;
        bool m_running = true;
        bool m_requestPlay = false;
        bool m_requestStop = false;
        std::string m_iniPath;
        EditorOptions m_options;
        int m_exitAfterFrames = -1;
        int m_frameCount = 0;
    };
}
