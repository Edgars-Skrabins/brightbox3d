#pragma once

namespace BrightBox3D::Editor
{
    class EditorApplication;

    enum class SizeSlider
    {
        Hidden,
        Shown
    };
}

namespace BrightBox3D::Editor::AssetsBottomBar
{
    float Height();
    void Draw(EditorApplication& application, SizeSlider slider);
}
