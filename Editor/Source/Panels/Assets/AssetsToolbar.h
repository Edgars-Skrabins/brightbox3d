#pragma once

#include "Panels/Assets/AssetsPanelState.h"

#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;
}

namespace BrightBox3D::Editor::AssetsToolbar
{
    void DrawCreateButton(EditorApplication& application, AssetsPanelState& state);
    void DrawSearch(AssetsPanelState& state);
    void DrawPanelMenu();
}
