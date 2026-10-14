#include "Widgets/InlineRename.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace BrightBox3D::Editor
{
    void InlineRename::Begin(const Core::Guid& target, const std::string& text)
    {
        m_target = target;
        m_buffer = text;
        m_focus = true;
    }

    std::optional<std::string> InlineRename::Draw(float width)
    {
        if (m_focus)
        {
            ImGui::SetKeyboardFocusHere();
            m_focus = false;
        }
        ImGui::SetNextItemWidth(width);
        if (ImGui::InputText("##rename", &m_buffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
        {
            m_target = {};
            return m_buffer;
        }
        if (!ImGui::IsItemActive() && !m_focus && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Escape)))
        {
            m_target = {};
        }
        return std::nullopt;
    }
}
