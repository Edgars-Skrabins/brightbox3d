#pragma once

#include <string>

namespace BrightBox3D::Editor
{
    class EditorContext;

    inline constexpr const char* BuiltinFolder = "Builtin";

    std::string ParentFolder(const std::string& path);
}

namespace BrightBox3D::Editor::AssetsFolderTree
{
    void Draw(EditorContext& context);
}
