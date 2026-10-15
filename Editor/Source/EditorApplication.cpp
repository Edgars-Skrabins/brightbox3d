#include "EditorApplication.h"

#include "Assets/AssetDatabase.h"
#include "Commands/CommandRegistry.h"
#include "Commands/CommandWidgets.h"
#include "Core/EngineInfo.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/Process.h"
#include "CreateMenuRegistry.h"
#include "DockLayout.h"
#include "EditorCommandLine.h"
#include "EditorCommands.h"
#include "EditorCreateMenu.h"
#include "EditorPlaySession.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "EditorWindows.h"
#include "GameBuilder.h"
#include "HotReload.h"
#include "ImGui/ImGuiPlatform.h"
#include "ImGui/ImGuiRenderer.h"
#include "Labels.h"
#include "Level/LevelSerializer.h"
#include "Panels/Assets/AssetFileImport.h"
#include "Panels/AssetsPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/LevelContentsPanel.h"
#include "Panels/LevelViewPanel.h"
#include "Panels/PreferencesPanel.h"
#include "Panels/ProjectSettingsPanel.h"
#include "Panels/TerminalPanel.h"
#include "Platform/Input.h"
#include "Platform/Shell.h"
#include "Rendering/DebugDraw.h"
#include "Rendering/TextureCapture.h"
#include "Runtime/FrameClock.h"
#include "Serialization/Json.h"
#include "StatusBar.h"
#include "Theme.h"
#include "Thumbnails.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <span>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr const char* UserSettingsFolder = ".brightbox3d";
        constexpr const char* PortableSettingsFolder = "UserSettings";
        constexpr const char* LayoutFileName = "EditorLayout.ini";
        constexpr const char* PreferencesFileName = "Preferences.json";
        constexpr int DefaultWindowWidth = 1600;
        constexpr int DefaultWindowHeight = 960;
        constexpr float PlayButtonWidth = 84.0f;
        constexpr float CompileAreaWidth = 300.0f;

        const ToolbarItem PlayToolbar[] = { CommandId::Play, CommandId::Pause, CommandId::Step };
        const ToolbarItem FileMenu[] = { CommandId::NewLevel, CommandId::Save, CommandId::BuildGame, ToolbarSeparator, CommandId::ProjectSettings, CommandId::OpenOtherProject, ToolbarSeparator, CommandId::Exit };
        const ToolbarItem EditMenu[] = { CommandId::Undo, CommandId::Redo, ToolbarSeparator, CommandId::Duplicate, CommandId::Delete, ToolbarSeparator, CommandId::Preferences };
        const ToolbarItem GameMenu[] = { CommandId::Play, CommandId::Compile };
        const ToolbarItem WindowMenu[] = { CommandId::NewLevelView, CommandId::NewLevelContents, CommandId::NewInspector, CommandId::NewAssets, CommandId::NewTerminal, ToolbarSeparator, CommandId::ResetLayout };
        constexpr EditorWindow DefaultPanels[] = { EditorWindow::LevelView, EditorWindow::LevelContents, EditorWindow::Inspector, EditorWindow::Assets, EditorWindow::Terminal };

        void DrawMenuItems(std::span<const ToolbarItem> items)
        {
            for (const ToolbarItem& item : items)
            {
                if (item.separator)
                {
                    ImGui::Separator();
                    continue;
                }
                CommandMenuItem(item.id);
            }
        }
    }

    EditorApplication::EditorApplication() = default;

    EditorApplication::~EditorApplication()
    {
        Shutdown();
    }

    std::filesystem::path EditorApplication::GetUserDirectory() const
    {
        std::filesystem::path home = Platform::Shell::UserDirectory();
        std::filesystem::path directory = home.empty() ? Core::FileSystem::ExecutableDirectory() / PortableSettingsFolder : home / UserSettingsFolder;
        Core::FileSystem::EnsureDirectory(directory);
        return directory;
    }

    bool EditorApplication::Initialize(int argc, char** argv)
    {
        std::string commandLineError;
        std::optional<EditorOptions> options = Core::ParseCommandLine<EditorOptions>(argc, argv, EditorCommandLine, commandLineError);
        if (!options)
        {
            Core::Log::Error(commandLineError);
            Core::Log::Info(Core::CommandLineUsage<EditorOptions>(std::filesystem::path(argv[0]).filename().string(), EditorCommandLine));
            return false;
        }
        m_options = *options;
        m_exitAfterFrames = m_options.frames;
        Core::Log::Info(Text(EditorText::EditorStarting, Core::EngineName));
        if (!CreateWindowAndDevice())
        {
            return false;
        }
        InitializeImGui();
        CreatePanels();
        SubscribeWindowEvents();
        m_recentProjects.Load(GetUserDirectory() / RecentProjects::FileName);
        ApplyCommandLine();
        return true;
    }

    bool EditorApplication::CreateWindowAndDevice()
    {
        Platform::WindowDesc desc;
        desc.title = Core::EngineName;
        desc.width = DefaultWindowWidth;
        desc.height = DefaultWindowHeight;
        desc.maximized = true;
        m_window = std::make_unique<Platform::Window>(desc);
        if (!m_window->IsValid())
        {
            return false;
        }
        m_renderContext = Runtime::RenderContext::Create(m_window.get(), Runtime::RenderContext::DefaultShaderDirectory());
        if (!m_renderContext)
        {
            Core::Log::Error(Text(EditorText::NoGraphicsDevice));
            return false;
        }
        Core::Log::Info(Text(EditorText::GraphicsDevice, GetDevice().GetName()));
        return true;
    }

    void EditorApplication::InitializeImGui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        m_iniPath = (GetUserDirectory() / LayoutFileName).string();
        MigrateEditorLayout(m_iniPath);
        io.IniFilename = m_iniPath.c_str();
        m_resetLayout = !std::filesystem::exists(m_iniPath);
        float contentScale = m_window->GetContentScale().x;
        RegisterEditorCommands(*this);
        RegisterEditorCreateMenu();
        EditorPreferences::Get().Load(GetUserDirectory() / PreferencesFileName);
        ApplyEditorTheme(1.0f);
        m_themeRevision = EditorPreferences::Get().GetRevision();
        LoadEditorFonts(contentScale);
        m_imguiPlatform = std::make_unique<ImGuiPlatform>(*m_window);
        m_imguiRenderer = std::make_unique<ImGuiRenderer>(GetDevice(), GetShaders());
    }

    void EditorApplication::CreatePanels()
    {
        m_thumbnails = std::make_unique<ThumbnailCache>(GetDevice(), GetRenderer());
        for (EditorWindow type : DefaultPanels)
        {
            CreatePanel(type);
        }
        m_projectSettings = std::make_unique<ProjectSettingsPanel>(*this);
        m_preferences = std::make_unique<PreferencesPanel>();
        m_playSession = std::make_unique<EditorPlaySession>(m_context, *this);
        m_context.GetLevelService().SetPlaySession(m_playSession.get());
        m_hotReload = std::make_unique<HotReload>(m_context);
        m_hotReload->OnCompileFailed = [this]
        {
            if (m_context.GetLevelService().IsPlaying())
            {
                Core::Log::Warning(Text(EditorText::PlayModeStoppedByCompileError));
                RequestStop();
            }
        };
        m_hotReload->OnReloaded = [this] { m_context.GetLevelService().BumpLevelRevision(); };
    }

    LevelViewPanel* EditorApplication::GetActiveLevelView()
    {
        return FindActivePanel<LevelViewPanel>(EditorWindow::LevelView);
    }

    LevelContentsPanel* EditorApplication::GetLevelContents()
    {
        return FindActivePanel<LevelContentsPanel>(EditorWindow::LevelContents);
    }

    AssetsPanel* EditorApplication::GetAssets()
    {
        for (const std::unique_ptr<EditorPanel>& panel : m_panels)
        {
            if (panel->GetWindowType() == EditorWindow::Assets && panel->IsOpen() && static_cast<AssetsPanel*>(panel.get())->GetState().menuTarget)
            {
                return static_cast<AssetsPanel*>(panel.get());
            }
        }
        return FindActivePanel<AssetsPanel>(EditorWindow::Assets);
    }

    Math::Vector3 EditorApplication::GetSpawnPoint()
    {
        LevelViewPanel* view = GetActiveLevelView();
        return view ? view->GetSpawnPoint() : Math::Vector3();
    }

    void EditorApplication::SetActivePanel(const EditorPanel& panel)
    {
        m_activePanels[static_cast<size_t>(panel.GetWindowType())] = panel.GetInstanceIndex();
    }

    int EditorApplication::NextInstanceIndex(EditorWindow type) const
    {
        int index = 1;
        while (std::ranges::any_of(m_panels, [&](const std::unique_ptr<EditorPanel>& panel) { return panel->IsOpen() && panel->GetWindowType() == type && panel->GetInstanceIndex() == index; }))
        {
            ++index;
        }
        return index;
    }

    EditorPanel& EditorApplication::CreatePanel(EditorWindow type)
    {
        int index = NextInstanceIndex(type);
        switch (type)
        {
        case EditorWindow::LevelView:
            m_panels.push_back(std::make_unique<LevelViewPanel>(*this, index));
            break;
        case EditorWindow::LevelContents:
            m_panels.push_back(std::make_unique<LevelContentsPanel>(*this, index));
            break;
        case EditorWindow::Inspector:
            m_panels.push_back(std::make_unique<InspectorPanel>(*this, index));
            break;
        case EditorWindow::Assets:
            m_panels.push_back(std::make_unique<AssetsPanel>(*this, index));
            break;
        default:
            m_panels.push_back(std::make_unique<TerminalPanel>(*this, index));
            break;
        }
        return *m_panels.back();
    }

    void EditorApplication::SubscribeWindowEvents()
    {
        m_window->OnFileDrop.Subscribe([this](const std::vector<std::filesystem::path>& files)
        {
            if (m_context.GetProjectService().HasProject())
            {
                if (AssetsPanel* assets = GetAssets())
                {
                    assets->ImportFiles(files);
                }
                else
                {
                    AssetFileImport::ImportFiles(files, "");
                }
            }
        });
        m_window->OnCloseRequested.Subscribe([this]
        {
            if (m_context.GetLevelService().IsPlaying())
            {
                m_playSession->Stop();
            }
        });
    }

    void EditorApplication::ApplyCommandLine()
    {
        std::filesystem::path projectPath = m_options.project;
        if (!m_options.createProject.empty())
        {
            std::string error;
            if (!Runtime::Project::IsProjectDirectory(m_options.createProject) && !Runtime::Project::Create(m_options.createProject, m_options.createProject.filename().string(), error))
            {
                Core::Log::Error(Text(EditorText::CreateProjectFailed, error));
            }
            projectPath = m_options.createProject;
        }
        if (!projectPath.empty())
        {
            OpenProject(projectPath);
        }
        if (!m_options.select.empty() && m_context.GetLevelService().GetEditLevel())
        {
            if (Level::LevelObject* selected = m_context.GetLevelService().GetEditLevel()->FindObjectByName(m_options.select))
            {
                m_context.GetSelectionService().Select(selected->GetId());
            }
        }
        if (m_options.build && m_context.GetProjectService().HasProject())
        {
            std::string error;
            bool built = GameBuilder::Build(m_context, GameBuilder::DefaultOutput(m_context), error);
            if (!built)
            {
                Core::Log::Error(Text(EditorText::BuildFailed, error));
            }
            m_exitAfterFrames = 1;
        }
        if (m_options.play)
        {
            RequestPlay();
        }
    }

    void EditorApplication::Shutdown()
    {
        if (!m_renderContext)
        {
            return;
        }
        GetDevice().WaitIdle();
        CommandRegistry::Get().Clear();
        CreateMenuRegistry::Get().Clear();
        if (m_playSession && m_playSession->IsActive())
        {
            m_playSession->Stop();
        }
        if (m_hotReload)
        {
            m_hotReload->OnProjectClosed();
        }
        m_fileChanges.Stop();
        m_context.GetProjectService().CloseProject();
        m_hotReload.reset();
        m_playSession.reset();
        m_projectSettings.reset();
        m_panels.clear();
        m_thumbnails.reset();
        m_imguiRenderer.reset();
        m_imguiPlatform.reset();
        if (ImGui::GetCurrentContext())
        {
            ImGui::DestroyContext();
        }
        m_renderContext.reset();
        m_window.reset();
        Platform::Window::ShutdownPlatform();
    }

    void EditorApplication::AddRecentProject(const std::filesystem::path& path)
    {
        m_recentProjects.Add(path);
        m_recentProjects.Save(GetUserDirectory() / RecentProjects::FileName);
    }

    bool EditorApplication::OpenProject(const std::filesystem::path& path)
    {
        if (m_context.GetProjectService().HasProject())
        {
            m_hotReload->OnProjectClosed();
            m_fileChanges.Stop();
            m_thumbnails->Clear();
            GetResources().Clear();
            m_context.GetProjectService().CloseProject();
        }
        std::string error;
        if (!m_context.GetProjectService().OpenProject(path, error))
        {
            m_launcher.SetError(error);
            Core::Log::Error(Text(EditorText::OpenProjectFailed, error));
            return false;
        }
        AddRecentProject(m_context.GetProjectService().GetProject().GetRoot());
        m_fileChanges.Watch(m_context.GetProjectService().GetProject().GetRoot());
        m_hotReload->OnProjectOpened();
        m_launcher.ClearError();
        Core::Log::Info(Text(EditorText::OpenedProject, m_context.GetProjectService().GetProject().GetName()));
        return true;
    }

    void EditorApplication::RequestPlay()
    {
        m_requestPlay = true;
    }

    void EditorApplication::RequestStop()
    {
        m_requestStop = true;
    }

    void EditorApplication::OpenProjectSettings()
    {
        m_projectSettings->Open();
    }

    void EditorApplication::OpenPreferences()
    {
        m_preferences->Open();
    }

    void EditorApplication::OpenNewLevelDialog()
    {
        m_newLevelDialog.Open();
    }

    void EditorApplication::BuildGame()
    {
        std::string error;
        if (!GameBuilder::Build(m_context, GameBuilder::DefaultOutput(m_context), error))
        {
            Core::Log::Error(Text(EditorText::BuildFailed, error));
            return;
        }
        RevealInFileBrowser(GameBuilder::DefaultOutput(m_context));
    }

    void EditorApplication::CloseProject()
    {
        if (m_context.GetLevelService().IsPlaying())
        {
            m_playSession->Stop();
        }
        m_hotReload->OnProjectClosed();
        m_fileChanges.Stop();
        m_thumbnails->Clear();
        GetResources().Clear();
        m_context.GetProjectService().CloseProject();
    }

    void EditorApplication::RequestExit()
    {
        m_running = false;
    }

    void EditorApplication::ResetLayout()
    {
        m_resetLayout = true;
        for (const std::unique_ptr<EditorPanel>& panel : m_panels)
        {
            if (panel->GetInstanceIndex() > 1)
            {
                panel->Close();
            }
        }
        for (EditorWindow type : DefaultPanels)
        {
            if (!FindPanel<EditorPanel>(type))
            {
                CreatePanel(type);
            }
        }
    }

    void EditorApplication::OpenFileInEditor(const std::string& path, int line)
    {
        Platform::Shell::OpenInCodeEditor(path, line);
    }

    void EditorApplication::RevealInFileBrowser(const std::filesystem::path& path)
    {
        Platform::Shell::RevealInFileBrowser(path);
    }

    void EditorApplication::UpdateTitle()
    {
        std::string title = Core::EngineName;
        if (m_context.GetProjectService().HasProject())
        {
            title += " - " + m_context.GetProjectService().GetProject().GetName();
            if (Level::Level* level = m_context.GetLevelService().GetEditLevel())
            {
                title += " - " + level->GetName() + (m_context.GetLevelService().IsDirty() ? "*" : "");
            }
            if (m_context.GetLevelService().IsPlaying())
            {
                title += Text(EditorText::PlayModeTitleSuffix);
            }
        }
        if (title != m_lastTitle)
        {
            m_window->SetTitle(title);
            m_lastTitle = title;
        }
    }

    void EditorApplication::DrawMainMenu()
    {
        if (!ImGui::BeginMainMenuBar())
        {
            return;
        }
        if (ImGui::BeginMenu(Text(EditorText::MenuFile)))
        {
            DrawMenuItems(FileMenu);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(Text(EditorText::MenuEdit)))
        {
            DrawMenuItems(EditMenu);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(Text(EditorText::MenuGame)))
        {
            DrawMenuItems(GameMenu);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(Text(EditorText::MenuWindow)))
        {
            DrawMenuItems(WindowMenu);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    void EditorApplication::DrawToolbar()
    {
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - ToolbarWidth(PlayToolbar, PlayButtonWidth)) * 0.5f);
        Editor::DrawToolbar(PlayToolbar, PlayButtonWidth);
        ImGui::SameLine(ImGui::GetWindowWidth() - CompileAreaWidth);
        CommandButton(m_hotReload->IsCompiling() ? CommandId::CancelCompile : CommandId::Compile);
        ImGui::SameLine();
        ImVec4 color = m_hotReload->GetState() == CompileState::Failed ? EditorColorVec4(EditorColor::Error) : (m_hotReload->IsCompiling() ? EditorColorVec4(EditorColor::Warning) : EditorColorVec4(EditorColor::Success));
        ImGui::TextColored(color, "%s", m_hotReload->GetStatusText().c_str());
    }

    void EditorApplication::DrawDockSpace()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        float toolbarHeight = ImGui::GetFrameHeight() + ScaledMetric(EditorMetric::ToolbarExtraHeight);
        float statusHeight = ImGui::GetFrameHeight();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbarHeight));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ScaledMetric(EditorMetric::ToolbarPaddingX), ScaledMetric(EditorMetric::ToolbarPaddingY)));
        ImGui::Begin("##Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar);
        DrawToolbar();
        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + toolbarHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - toolbarHeight - statusHeight));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##DockHost", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
        ImGui::PopStyleVar();
        ImGuiID dockspace = ImGui::GetID("MainDockSpace");
        if (m_resetLayout)
        {
            m_resetLayout = false;
            DockLayout::ApplyDefault(dockspace, ImGui::GetWindowSize());
        }
        ImGui::DockSpace(dockspace, ImVec2(0, 0), ImGuiDockNodeFlags_None);
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - statusHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusHeight));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ScaledMetric(EditorMetric::StatusBarPaddingX), ScaledMetric(EditorMetric::StatusBarPaddingY)));
        ImGui::Begin("##Status", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
        StatusBar::Draw(GetDevice().GetName(), GetRenderer().GetStatistics(), m_context.GetLevelService().GetActiveLevel());
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorApplication::Frame(float deltaTime)
    {
        m_fileChanges.Process(m_context.GetProjectService(), *m_hotReload);
        m_hotReload->Update();
        UpdatePlayMode(deltaTime);
        Rendering::DebugDraw::Advance(deltaTime);

        if (EditorPreferences::Get().GetRevision() != m_themeRevision)
        {
            m_themeRevision = EditorPreferences::Get().GetRevision();
            ApplyEditorTheme(1.0f);
        }
        m_imguiPlatform->NewFrame(deltaTime);
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        CommandRegistry::Get().BeginFrame();

        if (!m_context.GetProjectService().HasProject())
        {
            if (std::optional<std::filesystem::path> project = m_launcher.Draw())
            {
                OpenProject(*project);
            }
        }
        else
        {
            DrawEditor();
        }
        ImGui::Render();
        RenderFrame();
        Rendering::DebugDraw::Clear();
        UpdateTitle();
    }

    void EditorApplication::UpdatePlayMode(float deltaTime)
    {
        if (m_requestStop)
        {
            m_requestStop = false;
            m_requestPlay = false;
            m_playSession->Stop();
        }
        if (m_requestPlay)
        {
            m_requestPlay = false;
            if (!m_context.GetLevelService().IsPlaying() && m_context.GetLevelService().GetEditLevel())
            {
                if (m_context.GetLevelObjectEditService().IsEditingLevelObject())
                {
                    m_context.GetLevelObjectEditService().CloseLevelObjectEditor(true);
                }
                m_playSession->Start();
            }
        }
        if (m_context.GetLevelService().IsPlaying())
        {
            m_playSession->Tick(deltaTime);
            if (m_playSession->WantsStop())
            {
                m_playSession->Stop();
            }
        }
    }

    void EditorApplication::DrawEditor()
    {
        std::erase_if(m_panels, [](const std::unique_ptr<EditorPanel>& panel) { return !panel->IsOpen(); });
        DrawMainMenu();
        DrawDockSpace();
        for (size_t index = 0; index < m_panels.size(); ++index)
        {
            m_panels[index]->Draw();
        }
        m_projectSettings->Draw();
        m_preferences->Draw();
        m_newLevelDialog.Draw(m_context.GetLevelService());
        if (m_context.GetProjectService().HasProject())
        {
            CommandRegistry::Get().DispatchShortcuts(CommandScope::Global);
        }
    }

    void EditorApplication::RenderFrame()
    {
        GetResources().ProcessInvalidations();
        RHI::CommandList& commands = GetDevice().BeginFrame();
        m_thumbnails->Update(commands);
        for (const ViewRenderRequest& request : m_renderRequests)
        {
            GetRenderer().Render(commands, request.level, request.view, request.target);
        }
        m_renderRequests.clear();
        std::vector<RHI::Swapchain*> present;
        if (m_context.GetLevelService().IsPlaying())
        {
            m_playSession->Render(commands);
            present.push_back(m_playSession->GetSwapchain());
        }
        if (RHI::Texture* backbuffer = GetSwapchain().AcquireNextTexture())
        {
            m_imguiRenderer->Render(commands, ImGui::GetDrawData(), backbuffer);
            present.push_back(&GetSwapchain());
        }
        ++m_frameCount;
        bool capture = m_exitAfterFrames > 0 && m_frameCount == m_exitAfterFrames;
        if (capture)
        {
            SaveScreenshot(commands);
        }
        GetDevice().EndFrame(present);
        if (capture)
        {
            GetDevice().WaitIdle();
            m_running = false;
        }
    }

    void EditorApplication::SaveScreenshot(RHI::CommandList& commands)
    {
        auto save = [&](RHI::Texture* texture, const std::filesystem::path& path)
        {
            if (texture && !path.empty() && Rendering::SavePng(GetDevice(), *texture, path))
            {
                Core::Log::Info(Text(EditorText::SavedScreenshot, path.string()));
            }
        };
        RHI::TextureDesc desc;
        desc.width = std::max<uint32_t>(GetSwapchain().GetWidth(), 1);
        desc.height = std::max<uint32_t>(GetSwapchain().GetHeight(), 1);
        desc.format = RHI::Format::BGRA8Unorm;
        desc.usage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
        auto target = GetDevice().CreateTexture(desc);
        m_imguiRenderer->Render(commands, ImGui::GetDrawData(), target.get());
        std::unique_ptr<RHI::Texture> playTarget;
        if (m_context.GetLevelService().IsPlaying() && m_playSession->GetSwapchain() && !m_options.playScreenshot.empty())
        {
            desc.width = m_playSession->GetSwapchain()->GetWidth();
            desc.height = m_playSession->GetSwapchain()->GetHeight();
            playTarget = GetDevice().CreateTexture(desc);
            m_playSession->RenderTo(commands, playTarget.get());
        }
        GetDevice().EndFrame({});
        save(target.get(), m_options.screenshot);
        save(playTarget.get(), m_options.playScreenshot);
        GetDevice().BeginFrame();
    }

    void EditorApplication::Run()
    {
        Runtime::FrameClock clock;
        while (m_running && !m_window->ShouldClose())
        {
            Platform::Window::PollEvents();
            float deltaTime = clock.Tick();
            if (m_window->IsMinimized() && !m_context.GetLevelService().IsPlaying())
            {
                Runtime::FrameClock::WaitWhileMinimized();
                continue;
            }
            Frame(deltaTime);
        }
        if (m_context.GetProjectService().HasProject() && m_context.GetLevelService().IsDirty() && !m_context.GetLevelService().IsPlaying())
        {
            Core::Log::Info(Text(EditorText::SavingBeforeExit));
            m_context.GetLevelService().SaveLevel();
        }
    }
}
