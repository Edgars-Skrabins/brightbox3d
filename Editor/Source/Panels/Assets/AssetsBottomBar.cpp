#include "Panels/Assets/AssetsBottomBar.h"

#include "Assets/AssetDatabase.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "Panels/Assets/AssetActions.h"

#include <imgui.h>

#include <algorithm>

namespace BrightBox3D::Editor::AssetsBottomBar
{
    namespace
    {
        constexpr float SizeSliderWidth = 120.0f;
        constexpr int ListSliderGap = 16;
        constexpr int ListSliderStop = EditorPreferences::MinAssetIconSize - ListSliderGap;
        constexpr int ListSnapValue = ListSliderStop + ListSliderGap / 2;

        void DrawSizeSlider()
        {
            EditorPreferences& preferences = EditorPreferences::Get();
            int value = preferences.IsAssetListMode() ? ListSliderStop : preferences.GetAssetIconSize();
            ImGui::SetNextItemWidth(SizeSliderWidth);
            if (ImGui::SliderInt("##iconSize", &value, ListSliderStop, EditorPreferences::MaxAssetIconSize, ""))
            {
                preferences.SetAssetIconSize(value < ListSnapValue ? EditorPreferences::AssetListIconSize : value);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                preferences.Save();
            }
        }
    }

    float Height()
    {
        return ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.0f + ImGui::GetStyle().CellPadding.y;
    }

    void Draw(EditorApplication& application, bool showSizeSlider)
    {
        ImGui::Separator();
        SelectionService& selection = application.GetContext().GetSelectionService();
        const Assets::AssetRecord* record = selection.GetFocus() == SelectionFocus::Asset ? Assets::AssetDatabase::Get().FindRecord(selection.GetSelectedAsset()) : nullptr;
        float rowEnd = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", record ? AssetActions::DisplayPath(*record).c_str() : "");
        if (!showSizeSlider)
        {
            return;
        }
        ImGui::SameLine();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(std::max(cursor.x, rowEnd - SizeSliderWidth), cursor.y));
        DrawSizeSlider();
    }
}
