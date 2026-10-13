#include "Commands/CommandRegistry.h"

#include <algorithm>
#include <iterator>

namespace BrightBox3D::Editor
{
    namespace
    {
        size_t Index(CommandId id)
        {
            return static_cast<size_t>(id);
        }

        const EditorText ScopeLabels[] = { EditorText::CommandScopeGlobal, EditorText::LevelViewTitle, EditorText::LevelContentsTitle, EditorText::AssetsTitle, EditorText::InspectorTitle, EditorText::TerminalTitle };
        static_assert(std::size(ScopeLabels) == static_cast<size_t>(CommandScope::Count));

        ShortcutInput ImGuiShortcutInput()
        {
            return { [](const KeyChord& chord) { return ImGui::IsKeyChordPressed(chord.ToImGui()); }, ImGui::GetIO().WantTextInput };
        }
    }

    EditorText CommandScopeLabel(CommandScope scope)
    {
        return ScopeLabels[static_cast<size_t>(scope)];
    }

    CommandRegistry& CommandRegistry::Get()
    {
        static CommandRegistry* registry = new CommandRegistry();
        return *registry;
    }

    void CommandRegistry::Register(CommandId id, EditorCommand command)
    {
        m_shortcuts[Index(id)] = command.defaultShortcut;
        m_commands[Index(id)] = std::move(command);
    }

    void CommandRegistry::Clear()
    {
        m_commands = {};
        m_shortcuts = {};
        m_consumed.clear();
        m_suspended = false;
    }

    const EditorCommand& CommandRegistry::GetCommand(CommandId id) const
    {
        return m_commands[Index(id)];
    }

    std::string CommandRegistry::GetLabel(CommandId id) const
    {
        const EditorCommand& command = GetCommand(id);
        return command.dynamicLabel ? command.dynamicLabel() : std::string(Text(command.label));
    }

    Icon CommandRegistry::GetIcon(CommandId id) const
    {
        const EditorCommand& command = GetCommand(id);
        return command.dynamicIcon ? command.dynamicIcon() : command.icon;
    }

    bool CommandRegistry::IsEnabled(CommandId id) const
    {
        const EditorCommand& command = GetCommand(id);
        return command.execute && (!command.isEnabled || command.isEnabled());
    }

    bool CommandRegistry::IsChecked(CommandId id) const
    {
        const EditorCommand& command = GetCommand(id);
        return command.isChecked && command.isChecked();
    }

    bool CommandRegistry::Execute(CommandId id)
    {
        if (!IsEnabled(id))
        {
            return false;
        }
        GetCommand(id).execute();
        return true;
    }

    const KeyChord& CommandRegistry::GetShortcut(CommandId id) const
    {
        return m_shortcuts[Index(id)];
    }

    void CommandRegistry::SetShortcut(CommandId id, const KeyChord& chord)
    {
        m_shortcuts[Index(id)] = chord;
    }

    void CommandRegistry::ResetShortcut(CommandId id)
    {
        m_shortcuts[Index(id)] = GetCommand(id).defaultShortcut;
    }

    bool CommandRegistry::IsShortcutRebound(CommandId id) const
    {
        return !(m_shortcuts[Index(id)] == GetCommand(id).defaultShortcut);
    }

    std::vector<CommandId> CommandRegistry::FindConflicts(CommandId id, const KeyChord& chord) const
    {
        std::vector<CommandId> conflicts;
        if (!chord.IsBound())
        {
            return conflicts;
        }
        CommandScope scope = GetCommand(id).scope;
        for (size_t index = 0; index < m_commands.size(); ++index)
        {
            CommandScope otherScope = m_commands[index].scope;
            bool overlapping = otherScope == scope || otherScope == CommandScope::Global || scope == CommandScope::Global;
            if (index != Index(id) && m_commands[index].execute && overlapping && m_shortcuts[index] == chord)
            {
                conflicts.push_back(static_cast<CommandId>(index));
            }
        }
        return conflicts;
    }

    std::vector<CommandId> CommandRegistry::FindConflicts(CommandId id) const
    {
        return FindConflicts(id, GetShortcut(id));
    }

    std::optional<CommandId> CommandRegistry::FindCommand(std::string_view key) const
    {
        for (size_t index = 0; index < m_commands.size(); ++index)
        {
            if (m_commands[index].execute && key == m_commands[index].key)
            {
                return static_cast<CommandId>(index);
            }
        }
        return std::nullopt;
    }

    void CommandRegistry::BeginFrame()
    {
        m_consumed.clear();
    }

    void CommandRegistry::DispatchShortcuts(CommandScope scope)
    {
        DispatchShortcuts(scope, ImGuiShortcutInput());
    }

    void CommandRegistry::DispatchShortcuts(CommandScope scope, const ShortcutInput& input)
    {
        if (input.textInput || m_suspended)
        {
            return;
        }
        for (size_t index = 0; index < m_commands.size(); ++index)
        {
            const KeyChord& chord = m_shortcuts[index];
            if (m_commands[index].scope != scope || !chord.IsBound() || std::ranges::find(m_consumed, chord) != m_consumed.end() || !input.isPressed(chord))
            {
                continue;
            }
            m_consumed.push_back(chord);
            Execute(static_cast<CommandId>(index));
        }
    }
}
