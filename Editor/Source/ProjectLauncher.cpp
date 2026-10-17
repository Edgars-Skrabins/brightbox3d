#include "ProjectLauncher.h"

#include "Core/EngineInfo.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "Platform/Shell.h"
#include "RecentProjects.h"
#include "Runtime/Project.h"
#include "Theme.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr const char* DefaultProjectsFolder = "BrightBox3DProjects";
        constexpr float TitleFontScale = 2.0f;
    }

    ProjectLauncher::ProjectLauncher(const RecentProjects& recentProjects) : m_recentProjects(recentProjects)
    {
        std::filesystem::path home = Platform::Shell::UserDirectory();
        m_location = home.empty() ? std::string() : (home / DefaultProjectsFolder).generic_string();
    }

    std::optional<std::filesystem::path> ProjectLauncher::Draw()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("Launcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        float width = std::min(ScaledMetric(EditorMetric::LauncherMaxWidth), viewport->WorkSize.x - ScaledMetric(EditorMetric::LauncherMargin));
        ImGui::SetCursorPos(ImVec2((viewport->WorkSize.x - width) * 0.5f, ScaledMetric(EditorMetric::LauncherTopOffset)));
        ImGui::BeginChild("LauncherContent", ImVec2(width, 0), ImGuiChildFlags_None);
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * TitleFontScale);
        ImGui::TextUnformatted(Core::EngineName);
        ImGui::PopFont();
        ImGui::TextDisabled("%s", Text(EditorText::LauncherSubtitle));
        ImGui::Spacing();

        std::optional<std::filesystem::path> toOpen;
        DrawRecentProjects(toOpen);
        DrawNewProject(toOpen);
        DrawOpenProject(toOpen);
        if (!m_error.empty())
        {
            ImGui::TextColored(EditorColorVec4(EditorColor::Error), "%s", m_error.c_str());
        }
        ImGui::EndChild();
        ImGui::End();
        return toOpen;
    }

    void ProjectLauncher::DrawRecentProjects(std::optional<std::filesystem::path>& toOpen)
    {
        ImGui::SeparatorText(Text(EditorText::RecentProjects));
        if (m_recentProjects.GetProjects().empty())
        {
            ImGui::TextDisabled("%s", Text(EditorText::NoRecentProjects));
        }
        for (const std::string& path : m_recentProjects.GetProjects())
        {
            if (ImGui::Selectable(path.c_str()))
            {
                toOpen = path;
            }
        }
    }

    void ProjectLauncher::DrawNewProject(std::optional<std::filesystem::path>& toOpen)
    {
        ImGui::SeparatorText(Text(EditorText::NewProject));
        ImGui::InputText(Text(EditorText::Name), &m_name);
        ImGui::InputText(Text(EditorText::Location), &m_location);
        std::filesystem::path target = std::filesystem::path(m_location) / m_name;
        ImGui::TextDisabled("%s", target.generic_string().c_str());
        if (!ImGui::Button(Text(EditorText::CreateProject), ImVec2(ScaledMetric(EditorMetric::LauncherButtonWidth), 0)))
        {
            return;
        }
        if (m_name.empty())
        {
            m_error = Text(EditorText::EnterProjectName);
            return;
        }
        if (Core::Result<void> created = Runtime::Project::Create(target, m_name); !created)
        {
            m_error = created.GetError().GetText();
            return;
        }
        toOpen = target.generic_string();
    }

    void ProjectLauncher::DrawOpenProject(std::optional<std::filesystem::path>& toOpen)
    {
        ImGui::SeparatorText(Text(EditorText::OpenProject));
        ImGui::InputTextWithHint("##openpath", Text(EditorText::OpenProjectHint, Runtime::Project::FileName).c_str(), &m_openPath);
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::Open)))
        {
            toOpen = m_openPath;
        }
    }
}
