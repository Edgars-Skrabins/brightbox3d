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

    const AssetTypeStyle& GetAssetTypeStyle(Assets::AssetType type);
    Icon AssetTypeIcon(Assets::AssetType type, bool open);
    void DrawAssetLabel(const ImVec2& position, Assets::AssetType type, const std::string& name, bool open);
}
