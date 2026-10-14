#pragma once

namespace BrightBox3D::Editor
{
    class EditorApplication;
}

namespace BrightBox3D::Editor::AssetsBottomBar
{
    float Height();
    void Draw(EditorApplication& application, bool showSizeSlider);
}
