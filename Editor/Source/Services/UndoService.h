#pragma once

#include "Core/Guid.h"
#include "Serialization/Json.h"
#include "Services/UndoCommands.h"
#include "UndoStack.h"

#include <string>
#include <vector>

namespace BrightBox3D::Editor
{
    class EditorContext;

    class UndoService
    {
    public:
        explicit UndoService(EditorContext& context) : m_context(context) {}

        UndoStack& GetUndo() { return m_stack; }
        void Undo();
        void Redo();
        void Clear() { m_stack.Clear(); }
        UndoStack ExchangeStack(UndoStack stack);

        void RecordCreation(const std::vector<Level::LevelObject*>& objects, const std::string& name);
        void RecordDeletion(const std::vector<Level::LevelObject*>& objects);
        void RecordReparent(const Level::LevelObject& object, const ObjectLocation& from);
        void RecordObjectChange(const Core::Guid& id, const Serialization::JsonValue& before, const std::string& name);

    private:
        EditorContext& m_context;
        UndoStack m_stack;
    };
}
