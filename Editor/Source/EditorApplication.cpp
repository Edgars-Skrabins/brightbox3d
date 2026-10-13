#include "EditorApplication.h"

#include "Assets/AssetDatabase.h"
#include "Commands/CommandRegistry.h"
#include "Commands/CommandWidgets.h"
#include "Core/EngineInfo.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/Process.h"
#include "EditorCommandLine.h"
#include "EditorCommands.h"
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
#include "Theme.h"
#include "Thumbnails.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ImGuizmo.h>

#include <span>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr const char* UserSettingsFolder = ".brightbox3d";
        constexpr const char* PortableSettingsFolder = "UserSettings";
        constexpr const char* DefaultProjectsFolder = "BrightBox3DProjects";
        constexpr float PlayButtonWidth = 84.0f;
        constexpr float CompileAreaWidth = 300.0f;

        const ToolbarItem PlayToolbar[] = { CommandId::Play, CommandId::Pause, CommandId::Step };
        const ToolbarItem FileMenu[] = { CommandId::NewLevel, CommandId::Save, CommandId::BuildGame, ToolbarSeparator, CommandId::ProjectSettings, CommandId::OpenOtherProject, ToolbarSeparator, CommandId::Exit };
        const ToolbarItem EditMenu[] = { CommandId::Undo, CommandId::Redo, ToolbarSeparator, CommandId::Duplicate, CommandId::Delete, ToolbarSeparator, CommandId::Preferences };
        const ToolbarItem GameMenu[] = { CommandId::Play, CommandId::Compile };
        const ToolbarItem WindowMenu[] = { CommandId::ResetLayout };

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
        Platform::WindowDesc desc;
        desc.title = Core::EngineName;
        desc.width = 1600;
        desc.height = 960;
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

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        m_iniPath = (GetUserDirectory() / "EditorLayout.ini").string();
        MigrateEditorLayout(m_iniPath);
        io.IniFilename = m_iniPath.c_str();
        m_resetLayout = !std::filesystem::exists(m_iniPath);
        float contentScale = m_window->GetContentScale().x;
        RegisterEditorCommands(*this);
        EditorPreferences::Get().Load(GetUserDirectory() / "Preferences.json");
        ApplyEditorTheme(1.0f);
        m_themeRevision = EditorPreferences::Get().GetRevision();
        LoadEditorFonts(contentScale);
        m_imguiPlatform = std::make_unique<ImGuiPlatform>(*m_window);
        m_imguiRenderer = std::make_unique<ImGuiRenderer>(GetDevice(), GetShaders());

        m_thumbnails = std::make_unique<ThumbnailCache>(GetDevice(), GetRenderer());
        m_levelView = std::make_unique<LevelViewPanel>(*this);
        m_levelContents = std::make_unique<LevelContentsPanel>(*this);
        m_inspector = std::make_unique<InspectorPanel>(*this);
        m_assets = std::make_unique<AssetsPanel>(*this);
        m_terminal = std::make_unique<TerminalPanel>(*this);
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

        m_window->OnFileDrop.Subscribe([this](const std::vector<std::filesystem::path>& files)
        {
            if (m_context.GetProjectService().HasProject())
            {
                m_assets->ImportFiles(files);
            }
        });
        m_window->OnCloseRequested.Subscribe([this]
        {
            if (m_context.GetLevelService().IsPlaying())
            {
                m_playSession->Stop();
            }
        });

        m_recentProjects.Load(GetUserDirectory() / RecentProjects::FileName);
        std::filesystem::path home = Platform::Shell::UserDirectory();
        m_launcherLocation = home.empty() ? std::string() : (home / DefaultProjectsFolder).generic_string();
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
        return true;
    }

    void EditorApplication::Shutdown()
    {
        if (!m_renderContext)
        {
            return;
        }
        GetDevice().WaitIdle();
        CommandRegistry::Get().Clear();
        if (m_playSession && m_playSession->IsActive())
        {
            m_playSession->Stop();
        }
        if (m_hotReload)
        {
            m_hotReload->OnProjectClosed();
        }
        m_fileWatcher.Stop();
        m_context.GetProjectService().CloseProject();
        m_hotReload.reset();
        m_playSession.reset();
        m_projectSettings.reset();
        m_terminal.reset();
        m_assets.reset();
        m_inspector.reset();
        m_levelContents.reset();
        m_levelView.reset();
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
            m_fileWatcher.Stop();
            m_thumbnails->Clear();
            GetResources().Clear();
            m_context.GetProjectService().CloseProject();
        }
        std::string error;
        if (!m_context.GetProjectService().OpenProject(path, error))
        {
            m_launcherError = error;
            Core::Log::Error(Text(EditorText::OpenProjectFailed, error));
            return false;
        }
        AddRecentProject(m_context.GetProjectService().GetProject().GetRoot());
        m_fileWatcher.Watch(m_context.GetProjectService().GetProject().GetRoot());
        m_hotReload->OnProjectOpened();
        m_launcherError.clear();
        Core::Log::Info(Text(EditorText::OpenedProject, m_context.GetProjectService().GetProject().GetName()));
        return true;
    }

    void EditorApplication::ProcessFileChanges()
    {
        std::vector<Core::FileChange> changes = m_fileWatcher.Poll();
        if (changes.empty() || !m_context.GetProjectService().HasProject())
        {
            return;
        }
        std::vector<std::filesystem::path> assetChanges;
        std::string sourceRoot = m_context.GetProjectService().GetProject().GetSourceDirectory().generic_string();
        std::string assetsRoot = m_context.GetProjectService().GetProject().GetAssetsDirectory().generic_string();
        std::error_code error;
        std::string canonicalSource = std::filesystem::weakly_canonical(m_context.GetProjectService().GetProject().GetSourceDirectory(), error).generic_string();
        std::string canonicalAssets = std::filesystem::weakly_canonical(m_context.GetProjectService().GetProject().GetAssetsDirectory(), error).generic_string();
        for (const Core::FileChange& change : changes)
        {
            std::string path = change.path.generic_string();
            if (path.rfind(sourceRoot, 0) == 0 || path.rfind(canonicalSource, 0) == 0)
            {
                m_hotReload->NotifySourceChanged(change.path);
            }
            else if (path.rfind(assetsRoot, 0) == 0 || path.rfind(canonicalAssets, 0) == 0)
            {
                if (path.size() < 5 || (path.substr(path.size() - 5) != ".meta" && path.substr(path.size() - 4) != ".tmp"))
                {
                    assetChanges.push_back(change.path);
                }
            }
        }
        if (!assetChanges.empty())
        {
            Assets::AssetDatabase::Get().Refresh();
        }
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
        m_openNewLevelPopup = true;
        m_newLevelName = "NewLevel";
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
        m_fileWatcher.Stop();
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

    void EditorApplication::DrawLauncher()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("Launcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        float width = std::min(760.0f, viewport->WorkSize.x - 40.0f);
        ImGui::SetCursorPos(ImVec2((viewport->WorkSize.x - width) * 0.5f, 60.0f));
        ImGui::BeginChild("LauncherContent", ImVec2(width, 0), ImGuiChildFlags_None);
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 2.0f);
        ImGui::TextUnformatted(Core::EngineName);
        ImGui::PopFont();
        ImGui::TextDisabled("%s", Text(EditorText::LauncherSubtitle));
        ImGui::Spacing();

        ImGui::SeparatorText(Text(EditorText::RecentProjects));
        if (m_recentProjects.GetProjects().empty())
        {
            ImGui::TextDisabled("%s", Text(EditorText::NoRecentProjects));
        }
        std::string toOpen;
        for (const std::string& path : m_recentProjects.GetProjects())
        {
            if (ImGui::Selectable(path.c_str()))
            {
                toOpen = path;
            }
        }

        ImGui::SeparatorText(Text(EditorText::NewProject));
        ImGui::InputText(Text(EditorText::Name), &m_launcherName);
        ImGui::InputText(Text(EditorText::Location), &m_launcherLocation);
        std::filesystem::path target = std::filesystem::path(m_launcherLocation) / m_launcherName;
        ImGui::TextDisabled("%s", target.generic_string().c_str());
        if (ImGui::Button(Text(EditorText::CreateProject), ImVec2(160, 0)))
        {
            std::string error;
            if (m_launcherName.empty())
            {
                m_launcherError = Text(EditorText::EnterProjectName);
            }
            else if (Runtime::Project::Create(target, m_launcherName, error))
            {
                toOpen = target.generic_string();
            }
            else
            {
                m_launcherError = error;
            }
        }

        ImGui::SeparatorText(Text(EditorText::OpenProject));
        ImGui::InputTextWithHint("##openpath", Text(EditorText::OpenProjectHint, Runtime::Project::FileName).c_str(), &m_launcherPath);
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::Open)))
        {
            toOpen = m_launcherPath;
        }
        if (!m_launcherError.empty())
        {
            ImGui::TextColored(EditorColorVec4(EditorColor::Error), "%s", m_launcherError.c_str());
        }
        ImGui::EndChild();
        ImGui::End();
        if (!toOpen.empty())
        {
            OpenProject(toOpen);
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
        float toolbarHeight = ImGui::GetFrameHeight() + 10.0f;
        float statusHeight = ImGui::GetFrameHeight();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbarHeight));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 5));
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
            ImGui::DockBuilderRemoveNode(dockspace);
            ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetWindowSize());
            ImGuiID center = dockspace;
            ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.19f, nullptr, &center);
            ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.27f, nullptr, &center);
            ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);
            ImGui::DockBuilderDockWindow(WindowIdentity(EditorWindow::LevelContents).c_str(), left);
            ImGui::DockBuilderDockWindow(WindowIdentity(EditorWindow::Inspector).c_str(), right);
            ImGui::DockBuilderDockWindow(WindowIdentity(EditorWindow::Assets).c_str(), bottom);
            ImGui::DockBuilderDockWindow(WindowIdentity(EditorWindow::Terminal).c_str(), bottom);
            ImGui::DockBuilderDockWindow(WindowIdentity(EditorWindow::LevelView).c_str(), center);
            ImGui::DockBuilderFinish(dockspace);
        }
        ImGui::DockSpace(dockspace, ImVec2(0, 0), ImGuiDockNodeFlags_None);
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - statusHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusHeight));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
        ImGui::Begin("##Status", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
        DrawStatusBar();
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorApplication::DrawStatusBar()
    {
        const Rendering::RenderStatistics& stats = GetRenderer().GetStatistics();
        Level::Level* level = m_context.GetLevelService().GetActiveLevel();
        ImGui::TextDisabled("%s", Text(EditorText::StatusBar, GetDevice().GetName(), level ? level->GetObjectCount() : size_t{ 0 }, stats.drawCalls, stats.lights, ImGui::GetIO().Framerate).c_str());
        int errors = Core::Log::Count(Core::LogLevel::Error);
        if (errors > 0)
        {
            ImGui::SameLine();
            ImGui::TextColored(EditorColorVec4(EditorColor::Error), "%s", Text(EditorText::StatusBarErrors, errors).c_str());
        }
    }

    void EditorApplication::Frame(float deltaTime)
    {
        ProcessFileChanges();
        m_hotReload->Update();

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
            DrawLauncher();
        }
        else
        {
            DrawMainMenu();
            DrawDockSpace();
            m_levelView->Draw(deltaTime);
            m_levelContents->Draw();
            m_inspector->Draw();
            m_assets->Draw();
            m_terminal->Draw();
            m_projectSettings->Draw();
            m_preferences->Draw();
            if (m_openNewLevelPopup)
            {
                ImGui::OpenPopup(LabelWithId(EditorText::NewLevelTitle, "NewLevel").c_str());
                m_openNewLevelPopup = false;
            }
            if (ImGui::BeginPopupModal(LabelWithId(EditorText::NewLevelTitle, "NewLevel").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::InputText(Text(EditorText::Name), &m_newLevelName);
                if (ImGui::Button(Text(EditorText::Create), ImVec2(120, 0)) && !m_newLevelName.empty())
                {
                    if (m_context.GetLevelService().IsDirty())
                    {
                        m_context.GetLevelService().SaveLevel();
                    }
                    m_context.GetLevelService().NewLevel("Levels/" + m_newLevelName + ".level");
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(Text(EditorText::Cancel), ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (m_context.GetProjectService().HasProject())
            {
                CommandRegistry::Get().DispatchShortcuts(CommandScope::Global);
            }
        }
        ImGui::Render();

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
        Rendering::DebugDraw::Clear();
        UpdateTitle();
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
