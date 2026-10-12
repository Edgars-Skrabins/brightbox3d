#pragma once

#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class TerminalPanel
    {
    public:
        explicit TerminalPanel(EditorApplication& application);
        void Draw();

    private:
        EditorApplication& m_application;
        bool m_showInfo = true;
        bool m_showWarnings = true;
        bool m_showErrors = true;
        bool m_autoScroll = true;
        std::string m_filter;
        uint64_t m_lastRevision = 0;
    };
}
