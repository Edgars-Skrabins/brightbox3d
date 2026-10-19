#pragma once

#include "Assets/AssetType.h"
#include "EditorPreferences.h"
#include "IconGlyphs.h"

#include <imgui.h>

#include <string>

namespace BrightBox3D::Editor
{
    struct AssetTypeStyle
    {
        Icon icon = Icon::File;
        EditorColor color = EditorColor::AssetOther;
    };

    enum class FolderState
    {
        Closed,
        Open
    };

    const AssetTypeStyle& GetAssetTypeStyle(Assets::AssetType type);
    Icon AssetTypeIcon(Assets::AssetType type, FolderState folder);
    void DrawAssetLabel(const ImVec2& position, Assets::AssetType type, const std::string& name, FolderState folder);
    void DrawIconLabel(const ImVec2& position, Icon icon, EditorColor color, const std::string& name);
}
