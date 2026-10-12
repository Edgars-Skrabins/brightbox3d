#include "EditorApplication.h"

#include "Assets/AssetDatabase.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/Process.h"
#include "EditorPlaySession.h"
#include "HotReload.h"
#include "ImGui/ImGuiPlatform.h"
#include "ImGui/ImGuiRenderer.h"
#include "Level/LevelSerializer.h"
#include "Panels/AssetsPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/LevelContentsPanel.h"
#include "Panels/LevelViewPanel.h"
#include "Panels/ProjectSettingsPanel.h"
#include "Panels/TerminalPanel.h"
#include "Platform/Input.h"
#include "Rendering/DebugDraw.h"
#include "Serialization/Json.h"
#include "Theme.h"
#include "Thumbnails.h"
#include "UI/ScreenRenderer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ImGuizmo.h>

#include <thread>

namespace BrightBox3D::Editor
{
    EditorApplication::EditorApplication() = default;

    EditorApplication::~EditorApplication()
    {
        Shutdown();
    }

    std::filesystem::path EditorApplication::GetUserDirectory() const
    {
        const char* home = std::getenv("HOME");
#if defined(_WIN32)
        if (!home)
        {
            home = std::getenv("USERPROFILE");
        }
#endif
        std::filesystem::path directory = home ? std::filesystem::path(home) / ".brightbox3d" : Core::FileSystem::ExecutableDirectory() / "UserSettings";
        Core::FileSystem::EnsureDirectory(directory);
        return directory;
    }

    bool EditorApplication::Initialize(int argc, char** argv)
    {
        Core::Log::Info("BrightBox3D Editor starting");
        Platform::WindowDesc desc;
        desc.title = "BrightBox3D";
        desc.width = 1600;
        desc.height = 960;
        desc.maximized = true;
        m_window = std::make_unique<Platform::Window>(desc);
        if (!m_window->IsValid())
        {
            return false;
        }
        m_device = RHI::Device::Create();
        if (!m_device)
        {
            Core::Log::Error("No graphics device available");
            return false;
        }
        Core::Log::Info("Graphics: " + m_device->GetName());
        m_swapchain = m_device->CreateSwapchain(*m_window, true);
        m_shaders = std::make_unique<Rendering::ShaderLibrary>(*m_device, Core::FileSystem::EngineContentDirectory() / "Shaders");
        m_resources = std::make_unique<Rendering::GpuResourceCache>(*m_device);
        m_renderer = std::make_unique<Rendering::Renderer>(*m_device, *m_shaders, *m_resources);
        UI::ScreenRenderer::Get().Initialize(*m_device, *m_shaders, *m_resources);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        m_iniPath = (GetUserDirectory() / "EditorLayout.ini").string();
        io.IniFilename = m_iniPath.c_str();
        m_resetLayout = !std::filesystem::exists(m_iniPath);
        float contentScale = m_window->GetContentScale().x;
        ApplyEditorTheme(1.0f);
        LoadEditorFonts(contentScale);
        m_imguiPlatform = std::make_unique<ImGuiPlatform>(*m_window);
        m_imguiRenderer = std::make_unique<ImGuiRenderer>(*m_device, *m_shaders);

        m_thumbnails = std::make_unique<ThumbnailCache>(*m_device, *m_renderer);
        m_levelView = std::make_unique<LevelViewPanel>(*this);
        m_levelContents = std::make_unique<LevelContentsPanel>(*this);
        m_inspector = std::make_unique<InspectorPanel>(*this);
        m_assets = std::make_unique<AssetsPanel>(*this);
        m_terminal = std::make_unique<TerminalPanel>(*this);
        m_projectSettings = std::make_unique<ProjectSettingsPanel>(*this);
        m_playSession = std::make_unique<EditorPlaySession>(m_context, *this);
        m_context.SetPlaySession(m_playSession.get());
        m_hotReload = std::make_unique<HotReload>(m_context);
        m_hotReload->GetLevels = [this]
        {
            std::vector<Level::Level*> levels;
            if (m_context.GetEditLevel())
            {
                levels.push_back(m_context.GetEditLevel());
            }
            if (m_context.GetStashedLevel())
            {
                levels.push_back(m_context.GetStashedLevel());
            }
            if (m_playSession && m_playSession->IsActive())
            {
                levels.push_back(m_playSession->GetLevel());
            }
            return levels;
        };
        m_hotReload->OnCompileFailed = [this]
        {
            if (m_context.IsPlaying())
            {
                Core::Log::Warning("Stopping PlayMode because game code failed to compile");
                RequestStop();
            }
        };
        m_hotReload->OnReloaded = [this] { m_context.BumpLevelRevision(); };

        m_window->OnFileDrop.Subscribe([this](const std::vector<std::filesystem::path>& files)
        {
            if (m_context.HasProject())
            {
                m_assets->ImportFiles(files);
            }
        });
        m_window->OnCloseRequested.Subscribe([this]
        {
            if (m_context.IsPlaying())
            {
                m_playSession->Stop();
            }
        });

        LoadRecentProjects();
        const char* home = std::getenv("HOME");
        m_launcherLocation = home ? (std::filesystem::path(home) / "BrightBox3DProjects").generic_string() : "";
        if (argc > 1)
        {
            OpenProject(argv[1]);
        }
        return true;
    }

    void EditorApplication::Shutdown()
    {
        if (!m_device)
        {
            return;
        }
        m_device->WaitIdle();
        if (m_playSession && m_playSession->IsActive())
        {
            m_playSession->Stop();
        }
        if (m_hotReload)
        {
            m_hotReload->OnProjectClosed();
        }
        m_fileWatcher.Stop();
        m_context.CloseProject();
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
        UI::ScreenRenderer::Get().Shutdown();
        m_renderer.reset();
        m_resources.reset();
        m_shaders.reset();
        m_swapchain.reset();
        m_device.reset();
        m_window.reset();
        Platform::Window::ShutdownPlatform();
    }

    void EditorApplication::LoadRecentProjects()
    {
        m_recentProjects.clear();
        Serialization::JsonDocument document;
        if (Serialization::LoadJsonFile(GetUserDirectory() / "RecentProjects.json", document) && document.IsArray())
        {
            for (const Serialization::JsonValue& entry : document.GetArray())
            {
                if (entry.IsString() && Runtime::Project::IsProjectDirectory(entry.GetString()))
                {
                    m_recentProjects.push_back(entry.GetString());
                }
            }
        }
    }

    void EditorApplication::SaveRecentProjects()
    {
        Serialization::JsonDocument document;
        document.SetArray();
        for (const std::string& path : m_recentProjects)
        {
            document.PushBack(Serialization::MakeString(path, document.GetAllocator()), document.GetAllocator());
        }
        Serialization::SaveJsonFile(GetUserDirectory() / "RecentProjects.json", document);
    }

    void EditorApplication::AddRecentProject(const std::filesystem::path& path)
    {
        std::string text = path.generic_string();
        m_recentProjects.erase(std::remove(m_recentProjects.begin(), m_recentProjects.end(), text), m_recentProjects.end());
        m_recentProjects.insert(m_recentProjects.begin(), text);
        if (m_recentProjects.size() > 12)
        {
            m_recentProjects.resize(12);
        }
        SaveRecentProjects();
    }

    bool EditorApplication::OpenProject(const std::filesystem::path& path)
    {
        if (m_context.HasProject())
        {
            m_hotReload->OnProjectClosed();
            m_fileWatcher.Stop();
            m_thumbnails->Clear();
            m_resources->Clear();
            m_context.CloseProject();
        }
        std::string error;
        if (!m_context.OpenProject(path, error))
        {
            m_launcherError = error;
            Core::Log::Error("Could not open project: " + error);
            return false;
        }
        AddRecentProject(m_context.GetProject().GetRoot());
        m_fileWatcher.Watch(m_context.GetProject().GetRoot());
        m_hotReload->OnProjectOpened();
        m_launcherError.clear();
        Core::Log::Info("Opened project '" + m_context.GetProject().GetName() + "'");
        return true;
    }

    void EditorApplication::ProcessFileChanges()
    {
        std::vector<Core::FileChange> changes = m_fileWatcher.Poll();
        if (changes.empty() || !m_context.HasProject())
        {
            return;
        }
        std::vector<std::filesystem::path> assetChanges;
        std::string sourceRoot = m_context.GetProject().GetSourceDirectory().generic_string();
        std::string assetsRoot = m_context.GetProject().GetAssetsDirectory().generic_string();
        std::error_code error;
        std::string canonicalSource = std::filesystem::weakly_canonical(m_context.GetProject().GetSourceDirectory(), error).generic_string();
        std::string canonicalAssets = std::filesystem::weakly_canonical(m_context.GetProject().GetAssetsDirectory(), error).generic_string();
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

    void EditorApplication::OpenFileInEditor(const std::string& path, int line)
    {
        std::vector<std::string> command;
#if defined(__APPLE__)
        std::filesystem::path code = "/usr/local/bin/code";
        if (!std::filesystem::exists(code))
        {
            code = "/opt/homebrew/bin/code";
        }
        if (std::filesystem::exists(code))
        {
            command = { code.string(), "-g", path + ":" + std::to_string(std::max(line, 1)) };
        }
        else
        {
            command = { "open", "-t", path };
        }
#elif defined(_WIN32)
        command = { "cmd", "/c", "code", "-g", path + ":" + std::to_string(std::max(line, 1)) };
#else
        command = { "xdg-open", path };
#endif
        std::thread([command] { Core::Process::Run(command); }).detach();
    }

    void EditorApplication::RevealInFileBrowser(const std::filesystem::path& path)
    {
#if defined(__APPLE__)
        std::vector<std::string> command = { "open", "-R", path.string() };
#elif defined(_WIN32)
        std::vector<std::string> command = { "explorer", "/select," + path.string() };
#else
        std::vector<std::string> command = { "xdg-open", path.parent_path().string() };
#endif
        std::thread([command] { Core::Process::Run(command); }).detach();
    }

    void EditorApplication::UpdateTitle()
    {
        std::string title = "BrightBox3D";
        if (m_context.HasProject())
        {
            title += " - " + m_context.GetProject().GetName();
            if (Level::Level* level = m_context.GetEditLevel())
            {
                title += " - " + level->GetName() + (m_context.IsDirty() ? "*" : "");
            }
            if (m_context.IsPlaying())
            {
                title += " [PlayMode]";
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
        ImGui::TextUnformatted("BrightBox3D");
        ImGui::PopFont();
        ImGui::TextDisabled("Create or open a project");
        ImGui::Spacing();

        ImGui::SeparatorText("Recent Projects");
        if (m_recentProjects.empty())
        {
            ImGui::TextDisabled("No recent projects");
        }
        std::string toOpen;
        for (const std::string& path : m_recentProjects)
        {
            if (ImGui::Selectable(path.c_str()))
            {
                toOpen = path;
            }
        }

        ImGui::SeparatorText("New Project");
        ImGui::InputText("Name", &m_launcherName);
        ImGui::InputText("Location", &m_launcherLocation);
        std::filesystem::path target = std::filesystem::path(m_launcherLocation) / m_launcherName;
        ImGui::TextDisabled("%s", target.generic_string().c_str());
        if (ImGui::Button("Create Project", ImVec2(160, 0)))
        {
            std::string error;
            if (m_launcherName.empty())
            {
                m_launcherError = "Enter a project name";
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

        ImGui::SeparatorText("Open Project");
        ImGui::InputTextWithHint("##openpath", "Path to a folder containing Project.bbproject", &m_launcherPath);
        ImGui::SameLine();
        if (ImGui::Button("Open"))
        {
            toOpen = m_launcherPath;
        }
        if (!m_launcherError.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", m_launcherError.c_str());
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
        bool playing = m_context.IsPlaying();
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Level...", nullptr, false, !playing))
            {
                m_openNewLevelPopup = true;
                m_newLevelName = "NewLevel";
            }
            if (ImGui::MenuItem("Save", "Cmd+S", false, !playing))
            {
                m_context.SaveLevel();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Project Settings..."))
            {
                OpenProjectSettings();
            }
            if (ImGui::MenuItem("Open Other Project..."))
            {
                if (playing)
                {
                    m_playSession->Stop();
                }
                m_hotReload->OnProjectClosed();
                m_fileWatcher.Stop();
                m_thumbnails->Clear();
                m_resources->Clear();
                m_context.CloseProject();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
            {
                m_running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            std::string undo = "Undo " + m_context.GetUndo().GetUndoName();
            std::string redo = "Redo " + m_context.GetUndo().GetRedoName();
            if (ImGui::MenuItem(undo.c_str(), "Cmd+Z", false, m_context.GetUndo().CanUndo() && !playing))
            {
                m_context.Undo();
            }
            if (ImGui::MenuItem(redo.c_str(), "Cmd+Shift+Z", false, m_context.GetUndo().CanRedo() && !playing))
            {
                m_context.Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate", "Cmd+D", false, !m_context.GetSelection().empty()))
            {
                m_context.DuplicateSelection();
            }
            if (ImGui::MenuItem("Delete", "Del", false, !m_context.GetSelection().empty()))
            {
                m_context.DeleteSelection();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Game"))
        {
            if (ImGui::MenuItem(playing ? "Stop" : "Play", "Cmd+P"))
            {
                playing ? RequestStop() : RequestPlay();
            }
            if (ImGui::MenuItem("Compile Game Code", "Cmd+B", false, !m_hotReload->IsCompiling()))
            {
                m_hotReload->RequestCompile();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window"))
        {
            if (ImGui::MenuItem("Reset Layout"))
            {
                m_resetLayout = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    void EditorApplication::DrawToolbar()
    {
        bool playing = m_context.IsPlaying();
        float buttonWidth = 64.0f;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - buttonWidth * 3.0f) * 0.5f);
        if (playing)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.36f, 0.6f, 1.0f));
        }
        if (ImGui::Button(playing ? "Stop" : "Play", ImVec2(buttonWidth, 0)))
        {
            playing ? RequestStop() : RequestPlay();
        }
        if (playing)
        {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!playing);
        bool paused = m_playSession->IsPaused();
        if (ImGui::Button(paused ? "Resume" : "Pause", ImVec2(buttonWidth, 0)))
        {
            m_playSession->SetPaused(!paused);
        }
        ImGui::SameLine();
        if (ImGui::Button("Step", ImVec2(buttonWidth, 0)))
        {
            m_playSession->SetPaused(true);
            m_playSession->Step();
        }
        ImGui::EndDisabled();

        ImGui::SameLine(ImGui::GetWindowWidth() - 300.0f);
        ImGui::BeginDisabled(m_hotReload->IsCompiling());
        if (ImGui::Button("Compile"))
        {
            m_hotReload->RequestCompile();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImVec4 color = m_hotReload->GetState() == CompileState::Failed ? ImVec4(1.0f, 0.45f, 0.45f, 1.0f) : (m_hotReload->IsCompiling() ? ImVec4(1.0f, 0.82f, 0.35f, 1.0f) : ImVec4(0.6f, 0.8f, 0.6f, 1.0f));
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
            ImGui::DockBuilderDockWindow("Level Contents", left);
            ImGui::DockBuilderDockWindow("Inspector", right);
            ImGui::DockBuilderDockWindow("Assets", bottom);
            ImGui::DockBuilderDockWindow("Terminal", bottom);
            ImGui::DockBuilderDockWindow("Level View", center);
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
        const Rendering::RenderStatistics& stats = m_renderer->GetStatistics();
        Level::Level* level = m_context.GetActiveLevel();
        ImGui::TextDisabled("%s | %zu objects | %u draws | %u lights | %.1f fps", m_device->GetName().c_str(), level ? level->GetObjectCount() : 0, stats.drawCalls, stats.lights, ImGui::GetIO().Framerate);
        int errors = Core::Log::Count(Core::LogLevel::Error);
        if (errors > 0)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "| %d errors", errors);
        }
    }

    void EditorApplication::HandleShortcuts()
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput || !m_context.HasProject())
        {
            return;
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
        {
            m_context.SaveLevel();
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
        {
            m_context.Undo();
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z) || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y))
        {
            m_context.Redo();
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D))
        {
            m_context.DuplicateSelection();
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_P))
        {
            m_context.IsPlaying() ? RequestStop() : RequestPlay();
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_B))
        {
            m_hotReload->RequestCompile();
        }
        if ((ImGui::IsKeyPressed(ImGuiKey_Delete) || (ImGui::IsKeyPressed(ImGuiKey_Backspace) && io.KeyCtrl)) && !m_context.GetSelection().empty())
        {
            m_context.DeleteSelection();
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
            if (!m_context.IsPlaying() && m_context.GetEditLevel())
            {
                if (m_context.IsEditingLevelObject())
                {
                    m_context.CloseLevelObjectEditor(true);
                }
                m_playSession->Start();
            }
        }
        if (m_context.IsPlaying())
        {
            m_playSession->Tick(deltaTime);
            if (m_playSession->WantsStop())
            {
                m_playSession->Stop();
            }
        }
        Rendering::DebugDraw::Advance(deltaTime);

        m_imguiPlatform->NewFrame(deltaTime);
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        if (!m_context.HasProject())
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
            if (m_openNewLevelPopup)
            {
                ImGui::OpenPopup("New Level");
                m_openNewLevelPopup = false;
            }
            if (ImGui::BeginPopupModal("New Level", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::InputText("Name", &m_newLevelName);
                if (ImGui::Button("Create", ImVec2(120, 0)) && !m_newLevelName.empty())
                {
                    if (m_context.IsDirty())
                    {
                        m_context.SaveLevel();
                    }
                    m_context.NewLevel("Levels/" + m_newLevelName + ".level");
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            HandleShortcuts();
        }
        ImGui::Render();

        m_resources->ProcessInvalidations();
        RHI::CommandList& commands = m_device->BeginFrame();
        m_thumbnails->Update(commands);
        for (const ViewRenderRequest& request : m_renderRequests)
        {
            m_renderer->Render(commands, request.level, request.view, request.target);
        }
        m_renderRequests.clear();
        std::vector<RHI::Swapchain*> present;
        if (m_context.IsPlaying())
        {
            m_playSession->Render(commands);
            present.push_back(m_playSession->GetSwapchain());
        }
        if (RHI::Texture* backbuffer = m_swapchain->AcquireNextTexture())
        {
            m_imguiRenderer->Render(commands, ImGui::GetDrawData(), backbuffer);
            present.push_back(m_swapchain.get());
        }
        m_device->EndFrame(present);
        Rendering::DebugDraw::Clear();
        UpdateTitle();
    }

    void EditorApplication::Run()
    {
        double previous = Platform::Window::GetTime();
        while (m_running && !m_window->ShouldClose())
        {
            Platform::Window::PollEvents();
            double now = Platform::Window::GetTime();
            float deltaTime = static_cast<float>(std::min(now - previous, 0.1));
            previous = now;
            if (m_window->IsMinimized() && !m_context.IsPlaying())
            {
                Platform::Window::WaitEvents(0.1);
                continue;
            }
            Frame(deltaTime);
        }
        if (m_context.HasProject() && m_context.IsDirty() && !m_context.IsPlaying())
        {
            Core::Log::Info("Saving unsaved changes before exit");
            m_context.SaveLevel();
        }
    }
}
