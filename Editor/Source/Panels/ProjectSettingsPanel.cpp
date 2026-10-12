#include "Panels/ProjectSettingsPanel.h"

#include "EditorApplication.h"
#include "Panels/InspectorPanel.h"
#include "PropertyDrawer.h"

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
        ImGui::SetNextWindowSize(ImVec2(520, 480), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Project Settings", &m_open))
        {
            ImGui::End();
            return;
        }
        Runtime::Project& project = m_application.GetContext().GetProject();
        bool changed = false;
        ImGui::Text("Project: %s", project.GetName().c_str());
        ImGui::TextDisabled("%s", project.GetRoot().generic_string().c_str());
        ImGui::TextDisabled("Game module: %s", project.GetGameModuleName().c_str());

        ImGui::SeparatorText("Startup");
        Core::Guid startup = project.GetStartupLevel();
        PropertyDrawer::BeginRow("Startup Level", false);
        if (PropertyDrawer::AssetField("##startup", "Level", startup, false))
        {
            project.SetStartupLevel(startup);
            changed = true;
        }
        PropertyDrawer::EndRow();

        ImGui::SeparatorText("Default GameMode");
        Serialization::JsonDocument& gameMode = project.GetGameMode();
        changed |= InspectorPanel::DrawGameModeSettings(gameMode, gameMode.GetAllocator(), m_application.GetContext().IsPlaying());

        ImGui::SeparatorText("Simulation");
        float rate = 1.0f / project.GetFixedTimeStep();
        PropertyDrawer::BeginRow("Fixed Update Rate (Hz)", false);
        if (ImGui::DragFloat("##rate", &rate, 1.0f, 10.0f, 240.0f, "%.0f"))
        {
            project.SetFixedTimeStep(1.0f / std::max(rate, 1.0f));
            if (Level::Level* level = m_application.GetContext().GetEditLevel())
            {
                level->SetFixedDeltaTime(project.GetFixedTimeStep());
            }
            changed = true;
        }
        PropertyDrawer::EndRow();

        ImGui::SeparatorText("Hot Reload");
        bool autoReload = project.GetAutoHotReload();
        PropertyDrawer::BeginRow("Compile on save", false);
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
