#include "Panels/PreferencesPanel.h"

#include "EditorPreferences.h"
#include "Icons.h"
#include "PropertyDrawer.h"

#include <imgui.h>

#include <cstring>
#include <string>

namespace BrightBox3D::Editor
{
    namespace
    {
        const char* Categories[] = { "Interface", "Status", "Level View", "Asset Types" };
    }

    void PreferencesPanel::DrawColorRow(EditorColor color)
    {
        EditorPreferences& preferences = EditorPreferences::Get();
        const EditorColorInfo& info = EditorPreferences::GetColorInfo(color);
        bool isDefault = preferences.IsDefault(color);
        PropertyDrawer::BeginRow(info.label, false);
        float resetWidth = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - resetWidth - ImGui::GetStyle().ItemInnerSpacing.x);
        Math::Color value = preferences.GetColor(color);
        float components[4] = { value.r, value.g, value.b, value.a };
        if (ImGui::ColorEdit4("##color", components, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_DisplayHex))
        {
            preferences.SetColor(color, Math::Color(components[0], components[1], components[2], components[3]));
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            preferences.Save();
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::BeginDisabled(isDefault);
        if (ImGui::Button(Icons::RotateCcw, ImVec2(resetWidth, resetWidth)))
        {
            preferences.ResetColor(color);
            preferences.Save();
        }
        ImGui::EndDisabled();
        if (!isDefault && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Reset to default");
        }
        PropertyDrawer::EndRow();
    }

    void PreferencesPanel::Draw()
    {
        if (!m_open)
        {
            return;
        }
        ImGui::SetNextWindowSize(ImVec2(560, 640), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Preferences", &m_open))
        {
            ImGui::End();
            return;
        }
        EditorPreferences& preferences = EditorPreferences::Get();

        ImGui::SeparatorText("Accent");
        ImGui::TextDisabled("Used for selection, active controls, tabs and highlights.");
        DrawColorRow(EditorColor::Accent);

        for (const char* category : Categories)
        {
            ImGui::Spacing();
            if (!ImGui::CollapsingHeader(category, std::strcmp(category, "Interface") == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0))
            {
                continue;
            }
            ImGui::PushID(category);
            for (int i = 0; i < static_cast<int>(EditorColor::Count); ++i)
            {
                EditorColor color = static_cast<EditorColor>(i);
                if (color != EditorColor::Accent && std::strcmp(EditorPreferences::GetColorInfo(color).category, category) == 0)
                {
                    DrawColorRow(color);
                }
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::BeginDisabled(!preferences.HasChanges());
        if (ImGui::Button((std::string(Icons::RotateCcw) + "  Reset All Colors").c_str()))
        {
            ImGui::OpenPopup("Reset All Colors?");
        }
        ImGui::EndDisabled();
        if (ImGui::BeginPopupModal("Reset All Colors?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Every editor color will return to its default.");
            ImGui::Spacing();
            if (ImGui::Button("Reset"))
            {
                preferences.ResetAllColors();
                preferences.Save();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::End();
    }
}
