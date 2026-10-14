#include "Panels/TerminalPanel.h"

#include "Core/Log.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "EditorWindows.h"

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
        if (!ImGui::Begin(WindowTitle(EditorWindow::Terminal).c_str()))
        {
            ImGui::End();
            return;
        }
        if (ImGui::Button(Text(EditorText::Clear)))
        {
            Core::Log::Clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox(Text(EditorText::AutoScroll), &m_autoScroll);
        ImGui::SameLine();
        auto toggle = [](EditorText label, bool& value, int count, ImVec4 color)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, value ? color : EditorColorVec4(EditorColor::TextDim));
            std::string text = Text(label, count);
            if (ImGui::Button(text.c_str()))
            {
                value = !value;
            }
            ImGui::PopStyleColor();
        };
        toggle(EditorText::LogInfoCount, m_showInfo, Core::Log::Count(Core::LogLevel::Info, Core::LogSource::Game) + Core::Log::Count(Core::LogLevel::Info, Core::LogSource::Engine), EditorColorVec4(EditorColor::Text));
        ImGui::SameLine();
        toggle(EditorText::LogWarningCount, m_showWarnings, Core::Log::Count(Core::LogLevel::Warning, Core::LogSource::Game) + Core::Log::Count(Core::LogLevel::Warning, Core::LogSource::Engine), EditorColorVec4(EditorColor::Warning));
        ImGui::SameLine();
        toggle(EditorText::LogErrorCount, m_showErrors, Core::Log::Count(Core::LogLevel::Error, Core::LogSource::Game) + Core::Log::Count(Core::LogLevel::Error, Core::LogSource::Engine), EditorColorVec4(EditorColor::Error));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##filter", Text(EditorText::FilterHint), &m_filter);
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
            ImVec4 color = entry.level == Core::LogLevel::Error ? EditorColorVec4(EditorColor::Error) : (entry.level == Core::LogLevel::Warning ? EditorColorVec4(EditorColor::Warning) : EditorColorVec4(EditorColor::Text));
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
                ImGui::SetTooltip("%s", Text(EditorText::OpenLogSourceTooltip, entry.file, entry.line).c_str());
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    m_application.OpenFileInEditor(entry.file, entry.line);
                }
            }
            if (ImGui::BeginPopupContextItem("EntryContext"))
            {
                if (ImGui::MenuItem(Text(EditorText::Copy)))
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
