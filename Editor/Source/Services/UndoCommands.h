#pragma once

#include "Core/Guid.h"
#include "EditorText.h"
#include "Serialization/Json.h"
#include "UndoStack.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace BrightBox3D::Level
{
    class LevelObject;
}

namespace BrightBox3D::Editor
{
    struct ObjectLocation
    {
        Core::Guid id;
        Core::Guid parent;
        std::optional<size_t> collection;
        size_t sibling = 0;
        std::shared_ptr<Serialization::JsonDocument> snapshot;
    };

    ObjectLocation LocateObject(EditorContext& context, const Level::LevelObject& object, bool withSnapshot);

    class ObjectChangeCommand : public UndoCommand
    {
    public:
        ObjectChangeCommand(std::string name, ObjectLocation location, std::shared_ptr<Serialization::JsonDocument> before, std::shared_ptr<Serialization::JsonDocument> after);

        void Undo(EditorContext& context) override { Apply(context, *m_before); }
        void Redo(EditorContext& context) override { Apply(context, *m_after); }
        std::string GetName() const override { return m_name; }

    private:
        void Apply(EditorContext& context, const Serialization::JsonValue& state);

        std::string m_name;
        ObjectLocation m_location;
        std::shared_ptr<Serialization::JsonDocument> m_before;
        std::shared_ptr<Serialization::JsonDocument> m_after;
    };

    class ExistenceCommand : public UndoCommand
    {
    public:
        ExistenceCommand(std::string name, std::vector<ObjectLocation> objects, bool created) : m_name(std::move(name)), m_objects(std::move(objects)), m_created(created) {}

        void Undo(EditorContext& context) override { m_created ? Remove(context) : Restore(context); }
        void Redo(EditorContext& context) override { m_created ? Restore(context) : Remove(context); }
        std::string GetName() const override { return m_name; }

    private:
        void Remove(EditorContext& context);
        void Restore(EditorContext& context);

        std::string m_name;
        std::vector<ObjectLocation> m_objects;
        bool m_created;
    };

    class ReparentCommand : public UndoCommand
    {
    public:
        ReparentCommand(const Core::Guid& id, ObjectLocation from, ObjectLocation to) : m_id(id), m_from(std::move(from)), m_to(std::move(to)) {}

        void Undo(EditorContext& context) override { Apply(context, m_from); }
        void Redo(EditorContext& context) override { Apply(context, m_to); }
        std::string GetName() const override { return Text(EditorText::UndoReparent); }

    private:
        void Apply(EditorContext& context, const ObjectLocation& location);

        Core::Guid m_id;
        ObjectLocation m_from;
        ObjectLocation m_to;
    };
}
