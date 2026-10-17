#include "Panels/PreferencesPanel.h"

#include "Commands/CommandRegistry.h"
#include "EditorPreferences.h"
#include "EditorWindows.h"
#include "Labels.h"
#include "PropertyDrawer.h"
#include "Theme.h"

#include <imgui.h>

#include <string>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr const char* ResetAllColorsPopupId = "ResetAllColors";

        std::string CommandNames(const std::vector<CommandId>& commands)
        {
            std::string names;
            for (CommandId id : commands)
            {
                names += (names.empty() ? "" : Text(EditorText::ListSeparator)) + std::string(Text(CommandRegistry::Get().GetCommand(id).label));
            }
            return names;
        }
    }

    void PreferencesPanel::DrawColorRow(EditorColor color)
    {
        EditorPreferences& preferences = EditorPreferences::Get();
        bool isDefault = preferences.IsDefault(color);
        PropertyDrawer::BeginRow(EditorColorLabel(color).c_str(), RowLabel::Normal);
        float resetWidth = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - resetWidth - ImGui::GetStyle().ItemInnerSpacing.x);
        Math::Color value = preferences.GetColor(color);
        float components[4] = { value.r, value.g, value.b, value.a };
        if (ImGui::ColorEdit4("##color", components, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_DisplayHex))
        {
            preferences.SetColor(color, Math::Color(components[0], components[1], components[2], components[3]));
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            preferences.Save();
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::BeginDisabled(isDefault);
        if (IconButton(Icon::RotateCcw, EditorText::ResetToDefault))
        {
            preferences.ResetColor(color);
            preferences.Save();
        }
        ImGui::EndDisabled();
        PropertyDrawer::EndRow();
    }

    void PreferencesPanel::Draw()
    {
        if (!m_open)
        {
            m_capturing.reset();
            return;
        }
        ImGui::SetNextWindowSize(ImVec2(ScaledMetric(EditorMetric::PreferencesWindowWidth), ScaledMetric(EditorMetric::PreferencesWindowHeight)), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(WindowTitle(EditorWindow::Preferences).c_str(), &m_open))
        {
            ImGui::End();
            return;
        }
        if (ImGui::BeginTabBar("##PreferencesPages"))
        {
            if (ImGui::BeginTabItem(LabelWithId(EditorText::PreferencesColorsTab, "Colors").c_str()))
            {
                DrawColorsPage();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(LabelWithId(EditorText::PreferencesShortcutsTab, "Shortcuts").c_str()))
            {
                DrawShortcutsPage();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    void PreferencesPanel::DrawColorsPage()
    {
        EditorPreferences& preferences = EditorPreferences::Get();

        ImGui::SeparatorText(Text(EditorText::ColorAccent));
        ImGui::TextDisabled("%s", Text(EditorText::AccentColorHint));
        DrawColorRow(EditorColor::Accent);

        for (int categoryIndex = 0; categoryIndex < static_cast<int>(EditorColorCategory::Count); ++categoryIndex)
        {
            EditorColorCategory category = static_cast<EditorColorCategory>(categoryIndex);
            ImGui::Spacing();
            if (!ImGui::CollapsingHeader(Text(ColorCategoryLabel(category)), category == EditorColorCategory::Interface ? ImGuiTreeNodeFlags_DefaultOpen : 0))
            {
                continue;
            }
            ImGui::PushID(categoryIndex);
            for (int i = 0; i < static_cast<int>(EditorColor::Count); ++i)
            {
                EditorColor color = static_cast<EditorColor>(i);
                if (color != EditorColor::Accent && EditorPreferences::GetColorInfo(color).category == category)
                {
                    DrawColorRow(color);
                }
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::BeginDisabled(!preferences.HasChanges());
        if (ImGui::Button(IconLabel(Icon::RotateCcw, EditorText::ResetAllColors).c_str()))
        {
            ImGui::OpenPopup(LabelWithId(EditorText::ResetAllColorsQuestion, ResetAllColorsPopupId).c_str());
        }
        ImGui::EndDisabled();
        if (ImGui::BeginPopupModal(LabelWithId(EditorText::ResetAllColorsQuestion, ResetAllColorsPopupId).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted(Text(EditorText::ResetAllColorsWarning));
            ImGui::Spacing();
            if (ImGui::Button(Text(EditorText::Reset)))
            {
                preferences.ResetAllColors();
                preferences.Save();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(Text(EditorText::Cancel)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void PreferencesPanel::DrawShortcutsPage()
    {
        CommandRegistry& registry = CommandRegistry::Get();
        CaptureShortcut();
        for (int scopeIndex = 0; scopeIndex < static_cast<int>(CommandScope::Count); ++scopeIndex)
        {
            CommandScope scope = static_cast<CommandScope>(scopeIndex);
            std::vector<CommandId> commands;
            for (int index = 0; index < static_cast<int>(CommandId::Count); ++index)
            {
                CommandId id = static_cast<CommandId>(index);
                if (registry.GetCommand(id).execute && registry.GetCommand(id).scope == scope)
                {
                    commands.push_back(id);
                }
            }
            if (commands.empty() || !ImGui::CollapsingHeader(Text(CommandScopeLabel(scope)), ImGuiTreeNodeFlags_DefaultOpen))
            {
                continue;
            }
            for (CommandId id : commands)
            {
                DrawShortcutRow(id);
            }
        }
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button(IconLabel(Icon::RotateCcw, EditorText::ResetAllShortcuts).c_str()))
        {
            for (int index = 0; index < static_cast<int>(CommandId::Count); ++index)
            {
                registry.ResetShortcut(static_cast<CommandId>(index));
            }
            EditorPreferences::Get().Save();
        }
        DrawMoveShortcutPopup();
    }

    void PreferencesPanel::DrawShortcutRow(CommandId id)
    {
        CommandRegistry& registry = CommandRegistry::Get();
        const EditorCommand& command = registry.GetCommand(id);
        std::vector<CommandId> conflicts = registry.FindConflicts(id);
        bool capturing = m_capturing == id;
        ImGui::PushID(static_cast<int>(id));
        if (!conflicts.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColorVec4(EditorColor::Warning));
        }
        PropertyDrawer::BeginRow(IconLabel(command.icon, Text(command.label)).c_str(), RowLabel::Normal);
        float resetWidth = ImGui::GetFrameHeight();
        std::string chordText = ShortcutText(registry.GetShortcut(id));
        std::string label = capturing ? std::string(Text(EditorText::ShortcutCapturePrompt)) : (chordText.empty() ? std::string(Text(EditorText::ShortcutUnbound)) : chordText);
        if (capturing)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button((label + "###chord").c_str(), ImVec2(ImGui::GetContentRegionAvail().x - resetWidth - ImGui::GetStyle().ItemInnerSpacing.x, 0.0f)))
        {
            m_capturing = id;
        }
        if (capturing)
        {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", capturing ? Text(EditorText::ShortcutCaptureHint) : (conflicts.empty() ? label.c_str() : Text(EditorText::ShortcutConflict, CommandNames(conflicts)).c_str()));
        }
        if (!conflicts.empty())
        {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        bool rebound = registry.IsShortcutRebound(id);
        ImGui::BeginDisabled(!rebound);
        if (IconButton(Icon::RotateCcw, EditorText::ResetToDefault))
        {
            AssignShortcut(id, command.defaultShortcut);
        }
        ImGui::EndDisabled();
        PropertyDrawer::EndRow();
        ImGui::PopID();
    }

    void PreferencesPanel::CaptureShortcut()
    {
        if (!m_capturing)
        {
            return;
        }
        CommandRegistry::Get().SetShortcutsSuspended(true);
        for (int index = ImGuiKey_NamedKey_BEGIN; index < ImGuiKey_GamepadStart; ++index)
        {
            ImGuiKey key = static_cast<ImGuiKey>(index);
            if (!IsBindableKey(key) || !ImGui::IsKeyPressed(key, false))
            {
                continue;
            }
            CommandId id = *m_capturing;
            m_capturing.reset();
            if (key == ImGuiKey_Escape)
            {
                return;
            }
            AssignShortcut(id, key == ImGuiKey_Backspace ? KeyChord{} : KeyChord{ key, ImGui::GetIO().KeyMods });
            return;
        }
    }

    void PreferencesPanel::AssignShortcut(CommandId id, const KeyChord& chord)
    {
        CommandRegistry& registry = CommandRegistry::Get();
        std::vector<CommandId> conflicts = registry.FindConflicts(id, chord);
        if (!conflicts.empty())
        {
            m_pending = PendingShortcut{ id, chord, std::move(conflicts) };
            m_openMovePopup = true;
            return;
        }
        registry.SetShortcut(id, chord);
        EditorPreferences::Get().Save();
    }

    void PreferencesPanel::DrawMoveShortcutPopup()
    {
        std::string popupId = LabelWithId(EditorText::ShortcutInUseTitle, "MoveShortcut");
        if (m_openMovePopup)
        {
            ImGui::OpenPopup(popupId.c_str());
            m_openMovePopup = false;
        }
        if (!ImGui::BeginPopupModal(popupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        CommandRegistry& registry = CommandRegistry::Get();
        if (m_pending)
        {
            ImGui::TextUnformatted(Text(EditorText::ShortcutInUseWarning, ShortcutText(m_pending->chord), CommandNames(m_pending->conflicts), std::string(Text(registry.GetCommand(m_pending->id).label))).c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button(Text(EditorText::MoveShortcut)) && m_pending)
        {
            for (CommandId conflict : m_pending->conflicts)
            {
                registry.SetShortcut(conflict, {});
            }
            registry.SetShortcut(m_pending->id, m_pending->chord);
            EditorPreferences::Get().Save();
            m_pending.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::Cancel)))
        {
            m_pending.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
