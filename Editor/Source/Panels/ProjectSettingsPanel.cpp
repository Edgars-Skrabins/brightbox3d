#include "Panels/ProjectSettingsPanel.h"

#include "EditorApplication.h"
#include "EditorText.h"
#include "EditorWindows.h"
#include "Panels/InspectorPanel.h"
#include "PropertyDrawer.h"
#include "Theme.h"

#include <imgui.h>

namespace BrightBox3D::Editor
{
    ProjectSettingsPanel::ProjectSettingsPanel(EditorApplication& application) : m_application(application)
    {
    }

    void ProjectSettingsPanel::Draw()
    {
        if (!m_open)
        {
            return;
        }
        ImGui::SetNextWindowSize(ImVec2(ScaledMetric(EditorMetric::ProjectSettingsWindowWidth), ScaledMetric(EditorMetric::ProjectSettingsWindowHeight)), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(WindowTitle(EditorWindow::ProjectSettings).c_str(), &m_open))
        {
            ImGui::End();
            return;
        }
        Runtime::Project& project = m_application.GetContext().GetProjectService().GetProject();
        bool changed = false;
        ImGui::TextUnformatted(Text(EditorText::ProjectName, project.GetName()).c_str());
        ImGui::TextDisabled("%s", project.GetRoot().generic_string().c_str());
        ImGui::TextDisabled("%s", Text(EditorText::GameModuleName, project.GetGameModuleName()).c_str());

        ImGui::SeparatorText(Text(EditorText::SettingsStartup));
        Core::Guid startup = project.GetStartupLevel();
        PropertyDrawer::BeginRow(Text(EditorText::StartupLevel), false);
        if (PropertyDrawer::AssetField("##startup", Assets::AssetType::Level, startup, false))
        {
            project.SetStartupLevel(startup);
            changed = true;
        }
        PropertyDrawer::EndRow();

        ImGui::SeparatorText(Text(EditorText::SettingsSimulation));
        float rate = 1.0f / project.GetFixedTimeStep();
        PropertyDrawer::BeginRow(Text(EditorText::FixedUpdateRate), false);
        if (ImGui::DragFloat("##rate", &rate, 1.0f, 10.0f, 240.0f, "%.0f"))
        {
            project.SetFixedTimeStep(1.0f / std::max(rate, 1.0f));
            if (Level::Level* level = m_application.GetContext().GetLevelService().GetEditLevel())
            {
                level->SetFixedDeltaTime(project.GetFixedTimeStep());
            }
            changed = true;
        }
        PropertyDrawer::EndRow();

        ImGui::SeparatorText(Text(EditorText::SettingsHotReload));
        bool autoReload = project.GetAutoHotReload();
        PropertyDrawer::BeginRow(Text(EditorText::CompileOnSave), false);
        if (ImGui::Checkbox("##auto", &autoReload))
        {
            project.SetAutoHotReload(autoReload);
            changed = true;
        }
        PropertyDrawer::EndRow();

        if (changed)
        {
            project.Save();
        }
        ImGui::End();
    }
}
