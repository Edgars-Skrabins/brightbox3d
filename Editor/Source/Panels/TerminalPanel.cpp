#include "Panels/TerminalPanel.h"

#include "Core/Log.h"
#include "EditorApplication.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace BrightBox3D::Editor
{
    TerminalPanel::TerminalPanel(EditorApplication& application) : m_application(application)
    {
    }

    void TerminalPanel::Draw()
    {
        if (!ImGui::Begin("Terminal"))
        {
            ImGui::End();
            return;
        }
        if (ImGui::Button("Clear"))
        {
            Core::Log::Clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::SameLine();
        auto toggle = [](const char* label, bool& value, int count, ImVec4 color)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, value ? color : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            std::string text = std::string(label) + " " + std::to_string(count);
            if (ImGui::Button(text.c_str()))
            {
                value = !value;
            }
            ImGui::PopStyleColor();
        };
        toggle("Info", m_showInfo, Core::Log::Count(Core::LogLevel::Info), ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
        ImGui::SameLine();
        toggle("Warnings", m_showWarnings, Core::Log::Count(Core::LogLevel::Warning), ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
        ImGui::SameLine();
        toggle("Errors", m_showErrors, Core::Log::Count(Core::LogLevel::Error), ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##filter", "Filter", &m_filter);
        ImGui::Separator();

        ImGui::BeginChild("Entries", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        std::string lowerFilter = m_filter;
        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
        Core::Log::ForEach([&](const Core::LogEntry& entry)
        {
            if ((entry.level == Core::LogLevel::Info && !m_showInfo) || (entry.level == Core::LogLevel::Warning && !m_showWarnings) || (entry.level == Core::LogLevel::Error && !m_showErrors))
            {
                return;
            }
            if (!lowerFilter.empty())
            {
                std::string lower = entry.message + entry.file;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (lower.find(lowerFilter) == std::string::npos)
                {
                    return;
                }
            }
            ImVec4 color = entry.level == Core::LogLevel::Error ? ImVec4(1.0f, 0.45f, 0.45f, 1.0f) : (entry.level == Core::LogLevel::Warning ? ImVec4(1.0f, 0.82f, 0.35f, 1.0f) : ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
            ImGui::PushID(static_cast<int>(entry.serial));
            ImGui::TextDisabled("%s", entry.timestamp.c_str());
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            std::string text = entry.file.empty() ? entry.message : entry.file + ":" + std::to_string(entry.line) + ": " + entry.message;
            ImGui::TextUnformatted(text.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered() && !entry.file.empty())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetTooltip("Double-click to open %s:%d", entry.file.c_str(), entry.line);
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    m_application.OpenFileInEditor(entry.file, entry.line);
                }
            }
            if (ImGui::BeginPopupContextItem("EntryContext"))
            {
                if (ImGui::MenuItem("Copy"))
                {
                    ImGui::SetClipboardText(text.c_str());
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        });
        uint64_t revision = Core::Log::Revision();
        if (m_autoScroll && revision != m_lastRevision)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        m_lastRevision = revision;
        ImGui::EndChild();
        ImGui::End();
    }
}
