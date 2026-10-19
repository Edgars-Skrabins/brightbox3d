#pragma once

#include "Panels/Assets/AssetsPanelState.h"

namespace BrightBox3D::Editor
{
    class EditorApplication;
}

namespace BrightBox3D::Editor::EngineClassesView
{
    void DrawFolderTree(AssetsPanelState& state);
    void DrawBreadcrumbs(AssetsPanelState& state);
    void DrawItems(EditorApplication& application, AssetsPanelState& state);
    void DrawTree(EditorApplication& application, AssetsPanelState& state);
}
