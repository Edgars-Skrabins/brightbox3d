#pragma once

#include "Panels/Assets/AssetsPanelState.h"

#include <string>

namespace BrightBox3D::Assets
{
    struct AssetRecord;
}

namespace BrightBox3D::Editor
{
    class EditorApplication;

    inline constexpr const char* BuiltinFolder = "Builtin";

    std::string ParentFolder(const std::string& path);
    bool FoldersFirst(const Assets::AssetRecord& a, const Assets::AssetRecord& b);
}

namespace BrightBox3D::Editor::AssetsFolderTree
{
    void Draw(EditorApplication& application, AssetsPanelState& state);
}
