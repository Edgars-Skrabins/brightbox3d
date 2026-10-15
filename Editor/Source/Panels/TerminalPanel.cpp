#include "Panels/TerminalPanel.h"

#include "Core/Log.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "EditorWindows.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <iterator>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr const char* TerminalFilterIds[] = { "Info", "Warnings", "Errors", "Engine" };
        constexpr TerminalFilter LevelFilters[] = { TerminalFilter::Info, TerminalFilter::Warnings, TerminalFilter::Errors };

        static_assert(std::size(TerminalFilterIds) == static_cast<size_t>(TerminalFilter::Count));
        static_assert(std::size(LevelFilters) == static_cast<size_t>(Core::LogLevel::Count));
    }

    TerminalPanel::TerminalPanel(EditorApplication& application) : m_application(application)
    {
    }

    void TerminalPanel::DrawFilterToggle(TerminalFilter filter, const std::string& label, const ImVec4& color)
    {
        EditorPreferences& preferences = EditorPreferences::Get();
        bool on = preferences.IsTerminalFilterOn(filter);
        ImGui::PushStyleColor(ImGuiCol_Text, on ? color : EditorColorVec4(EditorColor::TextDim));
        if (ImGui::Button((label + "###" + TerminalFilterIds[static_cast<size_t>(filter)]).c_str()))
        {
            preferences.SetTerminalFilter(filter, !on);
            preferences.Save();
        }
        ImGui::PopStyleColor();
    }

    void TerminalPanel::DrawToolbar()
    {
        if (ImGui::Button(Text(EditorText::Clear)))
        {
            Core::Log::Clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox(Text(EditorText::AutoScroll), &m_autoScroll);
        ImGui::SameLine();
        DrawFilterToggle(TerminalFilter::Info, Text(EditorText::LogInfoCount, Core::Log::Count(Core::LogLevel::Info, Core::LogSource::Game)), EditorColorVec4(EditorColor::Text));
        ImGui::SameLine();
        DrawFilterToggle(TerminalFilter::Warnings, Text(EditorText::LogWarningCount, Core::Log::Count(Core::LogLevel::Warning, Core::LogSource::Game)), EditorColorVec4(EditorColor::Warning));
        ImGui::SameLine();
        DrawFilterToggle(TerminalFilter::Errors, Text(EditorText::LogErrorCount, Core::Log::Count(Core::LogLevel::Error, Core::LogSource::Game)), EditorColorVec4(EditorColor::Error));
        ImGui::SameLine();
        int engineWarnings = Core::Log::Count(Core::LogLevel::Warning, Core::LogSource::Engine);
        int engineErrors = Core::Log::Count(Core::LogLevel::Error, Core::LogSource::Engine);
        EditorColor engineColor = engineErrors > 0 ? EditorColor::Error : (engineWarnings > 0 ? EditorColor::Warning : EditorColor::Text);
        DrawFilterToggle(TerminalFilter::Engine, Text(EditorText::LogEngineCount, Core::Log::Count(Core::LogSource::Engine)), EditorColorVec4(engineColor));
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("%s", Text(EditorText::LogEngineTooltip, engineWarnings, engineErrors).c_str());
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##filter", Text(EditorText::FilterHint), &m_filter);
        ImGui::Separator();
    }

    bool TerminalPanel::IsVisible(const Core::LogEntry& entry) const
    {
        EditorPreferences& preferences = EditorPreferences::Get();
        if (entry.source == Core::LogSource::Engine)
        {
            return preferences.IsTerminalFilterOn(TerminalFilter::Engine);
        }
        return preferences.IsTerminalFilterOn(LevelFilters[static_cast<size_t>(entry.level)]);
    }

    void TerminalPanel::Draw()
    {
        if (!ImGui::Begin(WindowTitle(EditorWindow::Terminal).c_str()))
        {
            ImGui::End();
            return;
        }
        DrawToolbar();

        ImGui::BeginChild("Entries", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        std::string lowerFilter = m_filter;
        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
        Core::Log::ForEach([&](const Core::LogEntry& entry)
        {
            if (!IsVisible(entry))
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
            if (entry.source == Core::LogSource::Engine)
            {
                ImGui::TextDisabled("%s", Text(EditorText::LogEngineTag));
                ImGui::SameLine();
            }
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
