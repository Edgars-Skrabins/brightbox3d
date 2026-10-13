#pragma once

#include "Commands/EditorCommand.h"

#include <span>

namespace BrightBox3D::Editor
{
    struct ToolbarItem
    {
        constexpr ToolbarItem() = default;
        constexpr ToolbarItem(CommandId command) : id(command), separator(false) {}

        CommandId id = CommandId::Count;
        bool separator = true;
    };

    inline constexpr ToolbarItem ToolbarSeparator{};

    bool CommandButton(CommandId id, float width = 0.0f);
    bool CommandMenuItem(CommandId id);
    void DrawToolbar(std::span<const ToolbarItem> items, float buttonWidth = 0.0f);
    float ToolbarWidth(std::span<const ToolbarItem> items, float buttonWidth);
}
