#include "Panels/EditorPanel.h"

#include "EditorText.h"
#include "Theme.h"

#include <imgui_internal.h>

#include <string>

namespace BrightBox3D::Editor
{
    std::string EditorPanel::GetWindowName() const
    {
        if (m_instanceIndex <= 1)
        {
            return WindowTitle(m_type);
        }
        std::string index = std::to_string(m_instanceIndex);
        return Text(EditorText::PanelInstanceTitle, std::string(Text(GetWindowInfo(m_type).title)), index) + WindowIdentity(m_type) + "#" + index;
    }

    bool EditorPanel::BeginWindow(ImGuiWindowFlags flags)
    {
        ImGui::SetNextWindowSize(ImVec2(ScaledMetric(EditorMetric::FloatingPanelWidth), ScaledMetric(EditorMetric::FloatingPanelHeight)), ImGuiCond_FirstUseEver);
        if (m_focusRequested)
        {
            m_focusRequested = false;
            ImGui::SetNextWindowFocus();
        }
        ImGuiWindowClass windowClass;
        windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoCloseButton;
        ImGui::SetNextWindowClass(&windowClass);
        return ImGui::Begin(GetWindowName().c_str(), &m_open, flags);
    }
}
