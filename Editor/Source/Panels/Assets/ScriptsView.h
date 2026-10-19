#pragma once

#include "Panels/Assets/AssetsPanelState.h"

#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;
}

namespace BrightBox3D::Editor::ScriptsView
{
    void DrawFolderTree(EditorApplication& application, AssetsPanelState& state);
    void DrawBreadcrumbs(AssetsPanelState& state);
    void DrawItems(EditorApplication& application, AssetsPanelState& state);
    void DrawTree(EditorApplication& application, AssetsPanelState& state);
    void DrawSearchResults(EditorApplication& application, AssetsPanelState& state);
    void DrawCreateMenu(EditorApplication& application, AssetsPanelState& state, const std::string& folder);
    void DrawContextMenu(EditorApplication& application, AssetsPanelState& state, const AssetsTarget& target);
}
