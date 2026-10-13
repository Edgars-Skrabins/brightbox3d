#pragma once

namespace BrightBox3D::Editor
{
    enum class EditorColor;

    class PreferencesPanel
    {
    public:
        void Draw();
        void Open() { m_open = true; }

    private:
        void DrawColorRow(EditorColor color);

        bool m_open = false;
    };
}
