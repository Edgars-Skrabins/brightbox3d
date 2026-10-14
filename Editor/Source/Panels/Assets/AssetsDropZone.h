#pragma once

#include "Panels/Assets/AssetsPanelState.h"

#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;
}

namespace BrightBox3D::Editor::AssetsDropZone
{
    void Draw(EditorApplication& application, AssetsPanelState& state, const std::string& folder);
}
