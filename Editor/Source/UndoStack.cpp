#include "UndoStack.h"

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr size_t MaxCommands = 512;
    }

    void UndoStack::Push(std::unique_ptr<EditorCommand> command)
    {
        m_commands.resize(m_position);
        m_commands.push_back(std::move(command));
        if (m_commands.size() > MaxCommands)
        {
            m_commands.erase(m_commands.begin());
        }
        m_position = m_commands.size();
        ++m_revision;
    }

    void UndoStack::Undo(EditorContext& context)
    {
        if (!CanUndo())
        {
            return;
        }
        --m_position;
        m_commands[m_position]->Undo(context);
        ++m_revision;
    }

    void UndoStack::Redo(EditorContext& context)
    {
        if (!CanRedo())
        {
            return;
        }
        m_commands[m_position]->Redo(context);
        ++m_position;
        ++m_revision;
    }

    void UndoStack::Clear()
    {
        m_commands.clear();
        m_position = 0;
        ++m_revision;
    }

    std::string UndoStack::GetUndoName() const
    {
        return CanUndo() ? m_commands[m_position - 1]->GetName() : std::string();
    }

    std::string UndoStack::GetRedoName() const
    {
        return CanRedo() ? m_commands[m_position]->GetName() : std::string();
    }
}
