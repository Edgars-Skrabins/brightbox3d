#include "Services/UndoService.h"

#include "EditorContext.h"
#include "EditorText.h"

namespace BrightBox3D::Editor
{
    void UndoService::Undo()
    {
        if (!m_context.GetLevelService().IsPlaying())
        {
            m_stack.Undo(m_context);
            m_context.GetLevelService().BumpLevelRevision();
        }
    }

    void UndoService::Redo()
    {
        if (!m_context.GetLevelService().IsPlaying())
        {
            m_stack.Redo(m_context);
            m_context.GetLevelService().BumpLevelRevision();
        }
    }

    UndoStack UndoService::ExchangeStack(UndoStack stack)
    {
        std::swap(m_stack, stack);
        return stack;
    }

    void UndoService::RecordCreation(const std::vector<Level::LevelObject*>& objects, const std::string& name)
    {
        LevelService& levels = m_context.GetLevelService();
        if (levels.IsPlaying() || objects.empty())
        {
            return;
        }
        std::vector<ObjectLocation> locations;
        for (Level::LevelObject* object : objects)
        {
            locations.push_back(LocateObject(m_context, *object, true));
            levels.MarkDirty(*object);
        }
        m_stack.Push(std::make_unique<ExistenceCommand>(name, std::move(locations), true));
        levels.BumpLevelRevision();
    }

    void UndoService::RecordDeletion(const std::vector<Level::LevelObject*>& objects)
    {
        LevelService& levels = m_context.GetLevelService();
        if (levels.IsPlaying() || objects.empty())
        {
            return;
        }
        std::vector<ObjectLocation> locations;
        for (Level::LevelObject* object : objects)
        {
            locations.push_back(LocateObject(m_context, *object, true));
            levels.MarkDirty(*object);
        }
        m_stack.Push(std::make_unique<ExistenceCommand>(Text(EditorText::Delete), std::move(locations), false));
    }

    void UndoService::RecordReparent(const Level::LevelObject& object, const ObjectLocation& from)
    {
        if (!m_context.GetLevelService().IsPlaying())
        {
            m_stack.Push(std::make_unique<ReparentCommand>(object.GetId(), from, LocateObject(m_context, object, false)));
        }
    }

    void UndoService::RecordObjectChange(const Core::Guid& id, const Serialization::JsonValue& before, const std::string& name)
    {
        LevelService& levels = m_context.GetLevelService();
        Level::Level* level = levels.GetEditLevel();
        if (levels.IsPlaying() || !level)
        {
            return;
        }
        Level::LevelObject* object = level->FindObject(id);
        if (!object)
        {
            return;
        }
        auto beforeDocument = std::make_shared<Serialization::JsonDocument>();
        beforeDocument->CopyFrom(before, beforeDocument->GetAllocator());
        auto afterDocument = std::make_shared<Serialization::JsonDocument>();
        ObjectEditService::Snapshot(*object, *afterDocument);
        if (*beforeDocument == *afterDocument)
        {
            return;
        }
        m_stack.Push(std::make_unique<ObjectChangeCommand>(name, LocateObject(m_context, *object, false), beforeDocument, afterDocument));
        levels.MarkDirty(*object);
    }
}
