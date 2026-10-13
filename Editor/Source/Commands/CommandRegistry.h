#pragma once

#include "Commands/EditorCommand.h"

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace BrightBox3D::Editor
{
    EditorText CommandScopeLabel(CommandScope scope);

    struct ShortcutInput
    {
        std::function<bool(const KeyChord&)> isPressed;
        bool textInput = false;
    };

    class CommandRegistry
    {
    public:
        static CommandRegistry& Get();

        void Register(CommandId id, EditorCommand command);
        void Clear();
        const EditorCommand& GetCommand(CommandId id) const;

        std::string GetLabel(CommandId id) const;
        Icon GetIcon(CommandId id) const;
        bool IsEnabled(CommandId id) const;
        bool IsChecked(CommandId id) const;
        bool Execute(CommandId id);

        const KeyChord& GetShortcut(CommandId id) const;
        void SetShortcut(CommandId id, const KeyChord& chord);
        void ResetShortcut(CommandId id);
        bool IsShortcutRebound(CommandId id) const;
        std::vector<CommandId> FindConflicts(CommandId id, const KeyChord& chord) const;
        std::vector<CommandId> FindConflicts(CommandId id) const;
        std::optional<CommandId> FindCommand(std::string_view key) const;
        void SetShortcutsSuspended(bool suspended) { m_suspended = suspended; }

        void BeginFrame();
        void DispatchShortcuts(CommandScope scope);
        void DispatchShortcuts(CommandScope scope, const ShortcutInput& input);

    private:
        CommandRegistry() = default;

        std::array<EditorCommand, static_cast<size_t>(CommandId::Count)> m_commands;
        std::array<KeyChord, static_cast<size_t>(CommandId::Count)> m_shortcuts;
        std::vector<KeyChord> m_consumed;
        bool m_suspended = false;
    };
}
