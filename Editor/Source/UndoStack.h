#pragma once

#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Editor
{
    class EditorContext;

    class UndoCommand
    {
    public:
        virtual ~UndoCommand() = default;
        virtual void Undo(EditorContext& context) = 0;
        virtual void Redo(EditorContext& context) = 0;
        virtual std::string GetName() const = 0;
    };

    class UndoStack
    {
    public:
        void Push(std::unique_ptr<UndoCommand> command);
        bool CanUndo() const { return m_position > 0; }
        bool CanRedo() const { return m_position < m_commands.size(); }
        void Undo(EditorContext& context);
        void Redo(EditorContext& context);
        void Clear();
        std::string GetUndoName() const;
        std::string GetRedoName() const;
        uint64_t GetRevision() const { return m_revision; }

    private:
        std::vector<std::unique_ptr<UndoCommand>> m_commands;
        size_t m_position = 0;
        uint64_t m_revision = 0;
    };
}
