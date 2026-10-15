#pragma once

#include "Core/Log.h"
#include "EditorPreferences.h"
#include "Panels/EditorPanel.h"

#include <imgui.h>

#include <cstdint>
#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class TerminalPanel : public EditorPanel
    {
    public:
        TerminalPanel(EditorApplication& application, int instanceIndex);
        void Draw() override;

    private:
        void DrawToolbar();
        void DrawFilterToggle(TerminalFilter filter, const std::string& label, const ImVec4& color);
        bool IsVisible(const Core::LogEntry& entry) const;

        EditorApplication& m_application;
        bool m_autoScroll = true;
        std::string m_filter;
        uint64_t m_lastRevision = 0;
    };
}
