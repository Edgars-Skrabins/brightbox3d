#include "Panels/PreferencesPanel.h"

#include "EditorPreferences.h"
#include "EditorWindows.h"
#include "Labels.h"
#include "PropertyDrawer.h"

#include <imgui.h>

#include <string>

namespace BrightBox3D::Editor
{
    void PreferencesPanel::DrawColorRow(EditorColor color)
    {
        EditorPreferences& preferences = EditorPreferences::Get();
        const EditorColorInfo& info = EditorPreferences::GetColorInfo(color);
        bool isDefault = preferences.IsDefault(color);
        PropertyDrawer::BeginRow(Text(info.label), false);
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
        if (ImGui::Button(IconGlyph(Icon::RotateCcw), ImVec2(resetWidth, resetWidth)))
        {
            preferences.ResetColor(color);
            preferences.Save();
        }
        ImGui::EndDisabled();
        if (!isDefault && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Text(EditorText::ResetToDefault));
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
        if (!ImGui::Begin(WindowTitle(EditorWindow::Preferences).c_str(), &m_open))
        {
            ImGui::End();
            return;
        }
        EditorPreferences& preferences = EditorPreferences::Get();

        ImGui::SeparatorText(Text(EditorText::ColorAccent));
        ImGui::TextDisabled("%s", Text(EditorText::AccentColorHint));
        DrawColorRow(EditorColor::Accent);

        for (int categoryIndex = 0; categoryIndex < static_cast<int>(EditorColorCategory::Count); ++categoryIndex)
        {
            EditorColorCategory category = static_cast<EditorColorCategory>(categoryIndex);
            ImGui::Spacing();
            if (!ImGui::CollapsingHeader(Text(ColorCategoryLabel(category)), category == EditorColorCategory::Interface ? ImGuiTreeNodeFlags_DefaultOpen : 0))
            {
                continue;
            }
            ImGui::PushID(categoryIndex);
            for (int i = 0; i < static_cast<int>(EditorColor::Count); ++i)
            {
                EditorColor color = static_cast<EditorColor>(i);
                if (color != EditorColor::Accent && EditorPreferences::GetColorInfo(color).category == category)
                {
                    DrawColorRow(color);
                }
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::BeginDisabled(!preferences.HasChanges());
        if (ImGui::Button(IconLabel(Icon::RotateCcw, EditorText::ResetAllColors).c_str()))
        {
            ImGui::OpenPopup(LabelWithId(EditorText::ResetAllColorsQuestion, "ResetAllColors").c_str());
        }
        ImGui::EndDisabled();
        if (ImGui::BeginPopupModal(LabelWithId(EditorText::ResetAllColorsQuestion, "ResetAllColors").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted(Text(EditorText::ResetAllColorsWarning));
            ImGui::Spacing();
            if (ImGui::Button(Text(EditorText::Reset)))
            {
                preferences.ResetAllColors();
                preferences.Save();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(Text(EditorText::Cancel)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::End();
    }
}
