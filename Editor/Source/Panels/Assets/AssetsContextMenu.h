#pragma once

#include "Panels/Assets/AssetsPanelState.h"

#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;
}

namespace BrightBox3D::Editor::AssetsContextMenu
{
    void DrawCreateMenu(EditorApplication& application, const std::string& folder);
    void Draw(EditorApplication& application, AssetsPanelState& state, const AssetsTarget& target);
}
