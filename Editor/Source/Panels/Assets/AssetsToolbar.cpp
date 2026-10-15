#include "Panels/Assets/AssetsToolbar.h"

#include "Commands/CommandWidgets.h"
#include "EditorText.h"
#include "Labels.h"
#include "Panels/Assets/AssetsContextMenu.h"
#include "Theme.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace BrightBox3D::Editor::AssetsToolbar
{
    namespace
    {
        constexpr const char* CreateAssetPopupId = "CreateAsset";
        constexpr const char* PanelMenuPopupId = "AssetsPanelMenu";
        constexpr float MinSearchWidth = 160.0f;
    }

    void DrawCreateButton(EditorApplication& application, const std::string& folder)
    {
        if (ImGui::Button(Text(EditorText::CreateAssetButton)))
        {
            ImGui::OpenPopup(CreateAssetPopupId);
        }
        if (ImGui::BeginPopup(CreateAssetPopupId))
        {
            AssetsContextMenu::DrawCreateMenu(application, folder);
            ImGui::EndPopup();
        }
    }

    void DrawSearch(AssetsPanelState& state)
    {
        float menuWidth = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(std::max(MinSearchWidth, ImGui::GetContentRegionAvail().x - menuWidth));
        ImGui::InputTextWithHint("##filter", IconLabel(Icon::Search, EditorText::SearchHint).c_str(), &state.filter);
    }

    void DrawPanelMenu()
    {
        if (IconButton(Icon::EllipsisVertical, EditorText::AssetsPanelMenu))
        {
            ImGui::OpenPopup(PanelMenuPopupId);
        }
        if (ImGui::BeginPopup(PanelMenuPopupId))
        {
            CommandMenuItem(CommandId::AssetsOneColumnLayout);
            CommandMenuItem(CommandId::AssetsTwoColumnLayout);
            ImGui::Separator();
            CommandMenuItem(CommandId::AssetRefresh);
            ImGui::EndPopup();
        }
    }
}
