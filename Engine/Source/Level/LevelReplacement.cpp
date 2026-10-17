#include "Level/Level.h"

#include <algorithm>

namespace BrightBox3D::Level
{
    LevelObjectRuntimeState Level::GetRuntimeState(const LevelObject& object)
    {
        return { object.m_engineState, object.m_state };
    }

    void Level::SetRuntimeState(LevelObject& object, const LevelObjectRuntimeState& state)
    {
        object.m_engineState = state.engine;
        object.m_state = state.object;
    }

    ComponentRuntimeState Level::GetRuntimeState(const LevelObjectComponent& component)
    {
        return { component.m_engineState, component.m_state };
    }

    void Level::SetRuntimeState(LevelObjectComponent& component, const ComponentRuntimeState& state)
    {
        component.m_engineState = state.engine;
        component.m_state = state.component;
    }

    Reflection::EngineObjectState Level::GetRuntimeState(const Reflection::EngineObject& object)
    {
        return object.m_engineState;
    }

    void Level::SetRuntimeState(Reflection::EngineObject& object, const Reflection::EngineObjectState& state)
    {
        object.m_engineState = state;
    }

    std::unique_ptr<LevelObject> Level::ReleaseObjectForReplacement(LevelObject& object, std::vector<LevelObject*>& children, std::vector<std::unique_ptr<LevelObjectComponent>>& components)
    {
        for (const auto& component : object.m_components)
        {
            for (auto& subsystem : m_subsystems)
            {
                subsystem->OnComponentRemoved(*component);
            }
        }
        auto queued = std::find(m_pendingDestroy.begin(), m_pendingDestroy.end(), &object);
        if (queued != m_pendingDestroy.end())
        {
            m_pendingDestroy.erase(queued);
            m_replacedPendingDestroy.insert(object.GetId());
        }
        m_pendingComponentDestroy.erase(std::remove_if(m_pendingComponentDestroy.begin(), m_pendingComponentDestroy.end(), [&](LevelObjectComponent* component) { return component->m_owner == &object; }), m_pendingComponentDestroy.end());
        if (object.m_parent)
        {
            auto& siblings = object.m_parent->m_children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), &object), siblings.end());
        }
        else if (object.m_collection)
        {
            object.m_collection->RemoveRoot(&object);
        }
        children = std::move(object.m_children);
        object.m_children.clear();
        components = std::move(object.m_components);
        object.m_components.clear();
        m_index.erase(object.GetId());
        for (auto it = m_objects.begin(); it != m_objects.end(); ++it)
        {
            if (it->get() == &object)
            {
                std::unique_ptr<LevelObject> released = std::move(*it);
                m_objects.erase(it);
                return released;
            }
        }
        return nullptr;
    }

    void Level::InstallReplacement(std::unique_ptr<LevelObject> replacement, const std::vector<LevelObject*>& children, LevelObject* parent, LevelCollection* collection, std::optional<size_t> siblingPosition)
    {
        LevelObject* raw = replacement.get();
        raw->m_level = this;
        raw->m_collection = collection;
        raw->m_parent = parent;
        raw->m_transform.m_owner = raw;
        if (parent)
        {
            auto& siblings = parent->m_children;
            if (!siblingPosition || *siblingPosition > siblings.size())
            {
                siblings.push_back(raw);
            }
            else
            {
                siblings.insert(siblings.begin() + static_cast<std::ptrdiff_t>(*siblingPosition), raw);
            }
        }
        else if (collection)
        {
            collection->AddRoot(raw, siblingPosition);
        }
        for (LevelObject* child : children)
        {
            raw->m_children.push_back(child);
            child->m_parent = raw;
        }
        for (const auto& component : raw->m_components)
        {
            component->m_owner = raw;
            if (component->m_engineState.isPendingDestroy)
            {
                m_pendingComponentDestroy.push_back(component.get());
            }
            for (auto& subsystem : m_subsystems)
            {
                subsystem->OnComponentAdded(*component);
            }
        }
        if (m_replacedPendingDestroy.erase(raw->GetId()))
        {
            m_pendingDestroy.push_back(raw);
        }
        raw->m_transform.MarkDirty();
        m_index[raw->GetId()] = raw;
        m_objects.push_back(std::move(replacement));
        MarkHierarchyChanged();
    }

    std::unique_ptr<LevelObjectComponent> Level::ReleaseComponentForReplacement(LevelObject& object, size_t index)
    {
        LevelObjectComponent* component = object.m_components[index].get();
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnComponentRemoved(*component);
        }
        m_pendingComponentDestroy.erase(std::remove(m_pendingComponentDestroy.begin(), m_pendingComponentDestroy.end(), component), m_pendingComponentDestroy.end());
        std::unique_ptr<LevelObjectComponent> released = std::move(object.m_components[index]);
        object.m_components.erase(object.m_components.begin() + static_cast<std::ptrdiff_t>(index));
        return released;
    }

    void Level::InstallComponentReplacement(LevelObject& object, std::unique_ptr<LevelObjectComponent> component, size_t index)
    {
        LevelObjectComponent* raw = component.get();
        raw->m_owner = &object;
        index = std::min(index, object.m_components.size());
        object.m_components.insert(object.m_components.begin() + static_cast<std::ptrdiff_t>(index), std::move(component));
        if (raw->m_engineState.isPendingDestroy)
        {
            m_pendingComponentDestroy.push_back(raw);
        }
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnComponentAdded(*raw);
        }
        MarkHierarchyChanged();
    }

    std::unique_ptr<Reflection::EngineObject> Level::ReleaseEngineObjectForReplacement(size_t index)
    {
        return std::move(m_engineObjects[index]);
    }

    void Level::InstallEngineObjectReplacement(std::unique_ptr<Reflection::EngineObject> replacement, size_t index)
    {
        if (replacement)
        {
            replacement->m_level = this;
        }
        m_engineObjects[index] = std::move(replacement);
    }

    void Level::RemapReferences(const Reflection::ReferenceRemap& remap, const std::unordered_set<const Reflection::EngineObject*>& skip)
    {
        Reflection::ReferenceList unused;
        auto remapObject = [&](Reflection::EngineObject& object)
        {
            if (!skip.count(&object))
            {
                Reflection::ReferenceCarrier carrier(Reflection::ReferenceCarrier::Mode::Remap, unused, remap);
                object.CarryReferences(carrier);
            }
        };
        for (const auto& object : m_engineObjects)
        {
            if (object)
            {
                remapObject(*object);
            }
        }
        for (const auto& object : m_objects)
        {
            remapObject(*object);
            remapObject(object->GetTransform());
            for (const auto& component : object->m_components)
            {
                remapObject(*component);
            }
        }
        for (const auto& subsystem : m_subsystems)
        {
            remapObject(*subsystem);
        }
    }

    void Level::RemoveEmptyEngineObjects()
    {
        m_engineObjects.erase(std::remove(m_engineObjects.begin(), m_engineObjects.end(), nullptr), m_engineObjects.end());
    }
}
