#include "Commands/CommandWidgets.h"

#include "Commands/CommandRegistry.h"
#include "Labels.h"
#include "Theme.h"

#include <imgui.h>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr float SeparatorSpacing = 3.0f;

        std::string ButtonLabel(CommandId id)
        {
            CommandRegistry& registry = CommandRegistry::Get();
            Icon icon = registry.GetIcon(id);
            std::string label = registry.GetLabel(id);
            return (icon == Icon::None ? label : IconLabel(icon, label)) + "###" + registry.GetCommand(id).key;
        }

        void DrawTooltip(CommandId id)
        {
            CommandRegistry& registry = CommandRegistry::Get();
            const EditorCommand& command = registry.GetCommand(id);
            std::string tooltip = command.tooltip == EditorText::Count ? registry.GetLabel(id) : std::string(Text(command.tooltip));
            std::string shortcut = ShortcutText(registry.GetShortcut(id));
            if (shortcut.empty())
            {
                ImGui::SetTooltip("%s", tooltip.c_str());
                return;
            }
            ImGui::SetTooltip("%s  %s", tooltip.c_str(), shortcut.c_str());
        }

        ImVec4 CheckedColor(const EditorCommand& command)
        {
            return command.checkedColor ? EditorColorVec4(*command.checkedColor, 0.85f) : ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        }
    }

    bool CommandButton(CommandId id, float width, CommandButtonStyle style)
    {
        CommandRegistry& registry = CommandRegistry::Get();
        const EditorCommand& command = registry.GetCommand(id);
        bool checked = registry.IsChecked(id);
        if (checked)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, CheckedColor(command));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, command.checkedColor ? EditorColorVec4(*command.checkedColor) : ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
        }
        ImGui::BeginDisabled(!registry.IsEnabled(id));
        bool pressed = false;
        if (style == CommandButtonStyle::IconOnly)
        {
            float size = ImGui::GetFrameHeight();
            pressed = ImGui::Button((std::string("###") + command.key).c_str(), ImVec2(size, size));
            DrawCenteredIcon(registry.GetIcon(id), ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        }
        else
        {
            pressed = ImGui::Button(ButtonLabel(id).c_str(), ImVec2(width, 0.0f));
        }
        ImGui::EndDisabled();
        if (checked)
        {
            ImGui::PopStyleColor(2);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        {
            DrawTooltip(id);
        }
        return pressed && registry.Execute(id);
    }

    bool CommandMenuItem(CommandId id)
    {
        CommandRegistry& registry = CommandRegistry::Get();
        std::string label = registry.GetLabel(id) + "###" + registry.GetCommand(id).key;
        std::string shortcut = ShortcutText(registry.GetShortcut(id));
        bool pressed = ImGui::MenuItem(label.c_str(), shortcut.empty() ? nullptr : shortcut.c_str(), registry.IsChecked(id), registry.IsEnabled(id));
        return pressed && registry.Execute(id);
    }

    void DrawToolbar(std::span<const ToolbarItem> items, float buttonWidth)
    {
        float gap = -1.0f;
        bool first = true;
        for (const ToolbarItem& item : items)
        {
            if (item.separator)
            {
                gap = ImGui::GetStyle().ItemSpacing.x * SeparatorSpacing;
                continue;
            }
            if (!first)
            {
                ImGui::SameLine(0.0f, gap);
            }
            first = false;
            gap = -1.0f;
            CommandButton(item.id, buttonWidth);
        }
    }

    float ToolbarWidth(std::span<const ToolbarItem> items, float buttonWidth)
    {
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float width = 0.0f;
        float gap = 0.0f;
        for (const ToolbarItem& item : items)
        {
            if (item.separator)
            {
                gap = spacing * SeparatorSpacing;
                continue;
            }
            width += gap + buttonWidth;
            gap = spacing;
        }
        return width;
    }
}
