#pragma once

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class ProjectSettingsPanel
    {
    public:
        explicit ProjectSettingsPanel(EditorApplication& application);
        void Draw();
        void Open() { m_open = true; }

    private:
        EditorApplication& m_application;
        bool m_open = false;
    };
}
