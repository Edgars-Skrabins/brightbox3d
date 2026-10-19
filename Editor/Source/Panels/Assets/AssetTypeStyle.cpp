#include "Panels/Assets/AssetTypeStyle.h"

#include <iterator>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr float LabelIconGap = 2.0f;

        const AssetTypeStyle AssetTypeStyles[] = {
            { Icon::File, EditorColor::AssetOther },
            { Icon::Folder, EditorColor::AssetFolder },
            { Icon::Map, EditorColor::AssetLevel },
            { Icon::Layers, EditorColor::AssetCollection },
            { Icon::Package, EditorColor::AssetLevelObject },
            { Icon::LayoutTemplate, EditorColor::AssetTemplate },
            { Icon::Palette, EditorColor::AssetMaterial },
            { Icon::Box, EditorColor::AssetModel },
            { Icon::Image, EditorColor::AssetTexture },
            { Icon::Monitor, EditorColor::AssetUI },
            { Icon::Component, EditorColor::AssetUI },
            { Icon::FileCode, EditorColor::AssetStyleSheet },
            { Icon::Type, EditorColor::AssetOther }
        };

        static_assert(std::size(AssetTypeStyles) == static_cast<size_t>(Assets::AssetType::Count));
    }

    const AssetTypeStyle& GetAssetTypeStyle(Assets::AssetType type)
    {
        size_t index = static_cast<size_t>(type);
        return AssetTypeStyles[index < std::size(AssetTypeStyles) ? index : 0];
    }

    Icon AssetTypeIcon(Assets::AssetType type, FolderState folder)
    {
        return type == Assets::AssetType::Folder && folder == FolderState::Open ? Icon::FolderOpen : GetAssetTypeStyle(type).icon;
    }

    void DrawAssetLabel(const ImVec2& position, Assets::AssetType type, const std::string& name, FolderState folder)
    {
        DrawIconLabel(position, AssetTypeIcon(type, folder), GetAssetTypeStyle(type).color, name);
    }

    void DrawIconLabel(const ImVec2& position, Icon icon, EditorColor color, const std::string& name)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const char* glyph = IconGlyph(icon);
        drawList->AddText(position, EditorColorU32(color), glyph);
        ImVec2 namePosition(position.x + ImGui::CalcTextSize(glyph).x + ImGui::GetStyle().ItemInnerSpacing.x + LabelIconGap, position.y);
        drawList->AddText(namePosition, ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
    }
}
