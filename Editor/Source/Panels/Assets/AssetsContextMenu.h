#pragma once

#include "Core/Guid.h"

#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;
    class InlineRename;
}

namespace BrightBox3D::Editor::AssetsContextMenu
{
    void OpenAsset(EditorApplication& application, const Core::Guid& guid);
    void DrawCreateMenu(EditorApplication& application, const std::string& folder);
    void DrawItemMenu(EditorApplication& application, const Core::Guid& guid, InlineRename& rename, Core::Guid& pendingDelete);
}
