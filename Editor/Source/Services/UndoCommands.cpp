#include "Services/UndoCommands.h"

#include "EditorContext.h"

namespace BrightBox3D::Editor
{
    ObjectLocation LocateObject(EditorContext& context, const Level::LevelObject& object, bool withSnapshot)
    {
        ObjectLocation location;
        location.id = object.GetId();
        location.parent = object.GetParent() ? object.GetParent()->GetId() : Core::Guid{};
        location.collection = context.GetLevelService().GetCollectionIndex(object.GetCollection());
        location.sibling = object.GetSiblingIndex();
        if (withSnapshot)
        {
            location.snapshot = std::make_shared<Serialization::JsonDocument>();
            ObjectEditService::Snapshot(object, *location.snapshot);
        }
        return location;
    }

    ObjectChangeCommand::ObjectChangeCommand(std::string name, ObjectLocation location, std::shared_ptr<Serialization::JsonDocument> before, std::shared_ptr<Serialization::JsonDocument> after)
        : m_name(std::move(name)), m_location(std::move(location)), m_before(std::move(before)), m_after(std::move(after))
    {
    }

    void ObjectChangeCommand::Apply(EditorContext& context, const Serialization::JsonValue& state)
    {
        Level::Level* level = context.GetLevelService().GetEditLevel();
        Level::LevelObject* existing = level ? level->FindObject(m_location.id) : nullptr;
        Core::Guid parent = m_location.parent;
        int collection = m_location.collection;
        int sibling = m_location.sibling;
        if (existing)
        {
            parent = existing->GetParent() ? existing->GetParent()->GetId() : Core::Guid{};
            collection = context.GetLevelService().GetCollectionIndex(existing->GetCollection());
            sibling = existing->GetSiblingIndex();
        }
        context.GetObjectEditService().RestoreSnapshot(state, parent, collection, sibling);
    }

    void ExistenceCommand::Remove(EditorContext& context)
    {
        Level::Level* level = context.GetLevelService().GetEditLevel();
        for (auto it = m_objects.rbegin(); it != m_objects.rend(); ++it)
        {
            if (Level::LevelObject* object = level->FindObject(it->id))
            {
                context.GetLevelService().MarkDirty(*object);
                level->DestroyObjectImmediate(object);
            }
        }
        context.GetSelectionService().ClearSelection();
        context.GetLevelService().BumpLevelRevision();
    }

    void ExistenceCommand::Restore(EditorContext& context)
    {
        context.GetSelectionService().ClearSelection();
        for (const ObjectLocation& location : m_objects)
        {
            if (Level::LevelObject* object = context.GetObjectEditService().RestoreSnapshot(*location.snapshot, location.parent, location.collection, location.sibling))
            {
                context.GetSelectionService().Select(object->GetId(), true);
            }
        }
        context.GetLevelService().BumpLevelRevision();
    }

    void ReparentCommand::Apply(EditorContext& context, const ObjectLocation& location)
    {
        LevelService& levels = context.GetLevelService();
        Level::Level* level = levels.GetEditLevel();
        Level::LevelObject* object = level->FindObject(m_id);
        if (!object)
        {
            return;
        }
        levels.MarkDirty(*object);
        Level::LevelObject* parent = location.parent.IsValid() ? level->FindObject(location.parent) : nullptr;
        if (parent)
        {
            object->SetParent(parent, true, location.sibling);
        }
        else
        {
            object->SetParent(nullptr, true);
            level->SetRootCollection(*object, levels.GetCollectionByIndex(location.collection), location.sibling);
        }
        levels.MarkDirty(*object);
        levels.BumpLevelRevision();
    }
}
