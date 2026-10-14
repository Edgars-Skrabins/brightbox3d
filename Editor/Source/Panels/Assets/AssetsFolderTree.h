#pragma once

#include "Panels/Assets/AssetsPanelState.h"

#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;

    inline constexpr const char* BuiltinFolder = "Builtin";

    std::string ParentFolder(const std::string& path);
}

namespace BrightBox3D::Editor::AssetsFolderTree
{
    void Draw(EditorApplication& application, AssetsPanelState& state);
}
