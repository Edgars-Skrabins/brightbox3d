#include "Level/Level.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Level/LevelSerializer.h"

#include <algorithm>

namespace BrightBox3D::Level
{
    LevelObject* Level::CreateObject(const Reflection::TypeInfo& type, LevelCollection* collection, LevelObject* parent, const Core::Guid& id, bool initialize)
    {
        if (!type.IsA(LevelObject::StaticType()))
        {
            Core::Log::Error(Core::Text(Core::EngineText::NotLevelObjectType, type.GetName(), LevelObject::StaticType().GetName()));
            return nullptr;
        }
        std::unique_ptr<Reflection::EngineObject> created(type.CreateInstance());
        if (!created)
        {
            Core::Log::Error(Core::Text(Core::EngineText::TypeAbstract, type.GetName()));
            return nullptr;
        }
        std::unique_ptr<LevelObject> object(static_cast<LevelObject*>(created.release()));
        if (id.IsValid())
        {
            object->m_state.id = id;
        }
        object->m_state.name = type.GetName();
        return AdoptObject(std::move(object), collection, parent, initialize);
    }

    LevelObject* Level::AdoptObject(std::unique_ptr<LevelObject> object, LevelCollection* collection, LevelObject* parent, bool initialize)
    {
        if (m_index.count(object->GetId()))
        {
            Core::Log::Warning(Core::Text(Core::EngineText::ObjectIdCollision, object->GetName()));
            object->m_state.id = Core::Guid::Generate();
        }
        if (parent && parent->m_level != this)
        {
            parent = nullptr;
        }
        if (parent)
        {
            collection = parent->m_collection;
        }
        if (!collection)
        {
            collection = GetDefaultCollection();
        }

        LevelObject* raw = object.get();
        raw->m_level = this;
        raw->m_collection = collection;
        raw->m_parent = parent;
        raw->m_transform.m_owner = raw;
        if (parent)
        {
            parent->m_children.push_back(raw);
        }
        else
        {
            collection->AddRoot(raw);
        }
        raw->m_transform.MarkDirty();
        m_index[raw->GetId()] = raw;
        m_objects.push_back(std::move(object));

        for (const auto& component : raw->m_components)
        {
            component->m_owner = raw;
            for (auto& subsystem : m_subsystems)
            {
                subsystem->OnComponentAdded(*component);
            }
        }

        MarkHierarchyChanged();
        OnObjectCreated.Broadcast(*raw);
        if (initialize)
        {
            InitializeObject(*raw);
        }
        return raw;
    }

    void Level::InitializeObject(LevelObject& object)
    {
        object.OnInitialize();
        for (const auto& component : object.m_components)
        {
            component->OnInitialize();
        }
        if (m_isPlaying)
        {
            ActivateForPlay(object);
        }
    }

    void Level::ActivateForPlay(LevelObject& object)
    {
        if (!object.IsActiveInHierarchy() || object.m_engineState.isPendingDestroy)
        {
            return;
        }
        if (!object.m_engineState.hasLoaded)
        {
            object.m_engineState.hasLoaded = true;
            object.OnGameLoad();
        }
        for (size_t i = 0; i < object.m_components.size(); ++i)
        {
            LoadComponent(*object.m_components[i]);
        }
        ApplyEnabledState(object, true);
    }

    void Level::DestroyObject(LevelObject* object)
    {
        if (!object || object->m_level != this || object->m_engineState.isPendingDestroy)
        {
            return;
        }
        if (!m_isPlaying || m_isEnding)
        {
            DestroyObjectImmediate(object);
            return;
        }
        std::function<void(LevelObject&)> mark = [&](LevelObject& target)
        {
            target.m_engineState.isPendingDestroy = true;
            for (LevelObject* child : target.m_children)
            {
                mark(*child);
            }
        };
        mark(*object);
        m_pendingDestroy.push_back(object);
    }

    void Level::DestroyRecursive(LevelObject& object, bool callLifecycle, std::vector<LevelObject*>& order)
    {
        object.m_engineState.isPendingDestroy = true;
        std::vector<LevelObject*> children = object.m_children;
        for (LevelObject* child : children)
        {
            DestroyRecursive(*child, callLifecycle, order);
        }
        if (callLifecycle)
        {
            for (const auto& component : object.m_components)
            {
                if (component->m_engineState.isEnabledState)
                {
                    component->m_engineState.isEnabledState = false;
                    component->OnDisable();
                }
            }
            if (object.m_engineState.isEnabledState)
            {
                object.m_engineState.isEnabledState = false;
                object.OnDisable();
            }
            for (const auto& component : object.m_components)
            {
                component->OnDestroy();
            }
            object.OnDestroy();
        }
        order.push_back(&object);
    }

    void Level::FinalizeDestroy(const std::vector<LevelObject*>& order)
    {
        for (LevelObject* object : order)
        {
            OnObjectDestroyed.Broadcast(*object);
            for (const auto& component : object->m_components)
            {
                for (auto& subsystem : m_subsystems)
                {
                    subsystem->OnComponentRemoved(*component);
                }
            }
            if (object->m_parent)
            {
                auto& siblings = object->m_parent->m_children;
                siblings.erase(std::remove(siblings.begin(), siblings.end(), object), siblings.end());
            }
            else if (object->m_collection)
            {
                object->m_collection->RemoveRoot(object);
            }
            object->m_children.clear();
            m_index.erase(object->GetId());
        }
        std::unordered_map<LevelObject*, bool> doomed;
        for (LevelObject* object : order)
        {
            doomed[object] = true;
        }
        m_pendingDestroy.erase(std::remove_if(m_pendingDestroy.begin(), m_pendingDestroy.end(), [&](LevelObject* object) { return doomed.count(object) > 0; }), m_pendingDestroy.end());
        m_pendingComponentDestroy.erase(std::remove_if(m_pendingComponentDestroy.begin(), m_pendingComponentDestroy.end(), [&](LevelObjectComponent* component) { return doomed.count(component->GetOwner()) > 0; }), m_pendingComponentDestroy.end());
        m_objects.erase(std::remove_if(m_objects.begin(), m_objects.end(), [&](const auto& entry) { return doomed.count(entry.get()) > 0; }), m_objects.end());
        MarkHierarchyChanged();
    }

    void Level::DestroyObjectImmediate(LevelObject* object)
    {
        if (!object || object->m_level != this)
        {
            return;
        }
        std::vector<LevelObject*> order;
        DestroyRecursive(*object, m_isPlaying, order);
        FinalizeDestroy(order);
    }

    void Level::FlushDestroyed()
    {
        for (auto it = m_engineObjects.begin(); it != m_engineObjects.end();)
        {
            if (*it && (*it)->m_engineState.isPendingDestroy)
            {
                std::unique_ptr<Reflection::EngineObject> doomed = std::move(*it);
                it = m_engineObjects.erase(it);
                if (doomed->m_engineState.hasLoaded)
                {
                    doomed->OnGameEnd();
                }
                doomed->OnDestroy();
            }
            else
            {
                ++it;
            }
        }
        if (!m_pendingComponentDestroy.empty())
        {
            std::vector<LevelObjectComponent*> components;
            components.swap(m_pendingComponentDestroy);
            for (LevelObjectComponent* component : components)
            {
                LevelObject* owner = component->m_owner;
                if (!owner || owner->m_engineState.isPendingDestroy)
                {
                    continue;
                }
                if (component->m_engineState.isEnabledState)
                {
                    component->m_engineState.isEnabledState = false;
                    component->OnDisable();
                }
                component->OnDestroy();
                for (auto& subsystem : m_subsystems)
                {
                    subsystem->OnComponentRemoved(*component);
                }
                owner->DetachComponent(component);
            }
        }
        if (!m_pendingDestroy.empty())
        {
            std::vector<LevelObject*> roots = m_pendingDestroy;
            std::vector<LevelObject*> order;
            for (LevelObject* root : roots)
            {
                bool nested = false;
                for (LevelObject* other : roots)
                {
                    if (other != root && root->IsDescendantOf(other))
                    {
                        nested = true;
                        break;
                    }
                }
                if (!nested)
                {
                    DestroyRecursive(*root, true, order);
                }
            }
            FinalizeDestroy(order);
        }
    }

    Reflection::EngineObject* Level::CreateEngineObject(const Reflection::TypeInfo& type)
    {
        if (type.IsA(LevelObject::StaticType()) || type.IsA(LevelObjectComponent::StaticType()))
        {
            Core::Log::Error(Core::Text(Core::EngineText::WorldTypeAsEngineObject, type.GetName(), LevelObject::StaticType().GetName()));
            return nullptr;
        }
        std::unique_ptr<Reflection::EngineObject> object(type.CreateInstance());
        if (!object)
        {
            Core::Log::Error(Core::Text(Core::EngineText::TypeAbstract, type.GetName()));
            return nullptr;
        }
        Reflection::EngineObject* raw = object.get();
        raw->m_level = this;
        m_engineObjects.push_back(std::move(object));
        if (m_isPlaying)
        {
            LoadEngineObject(*raw);
        }
        return raw;
    }

    void Level::DestroyEngineObject(Reflection::EngineObject* object)
    {
        auto it = std::find_if(m_engineObjects.begin(), m_engineObjects.end(), [&](const auto& entry) { return entry.get() == object; });
        if (it == m_engineObjects.end() || object->m_engineState.isPendingDestroy)
        {
            return;
        }
        object->m_engineState.isPendingDestroy = true;
        if (!m_isPlaying)
        {
            std::unique_ptr<Reflection::EngineObject> doomed = std::move(*it);
            m_engineObjects.erase(it);
            doomed->OnDestroy();
        }
    }

    LevelObject* Level::Spawn(const Assets::AssetReference<LevelObjectVariantAsset>& variant, const Math::Vector3& position, const Math::Quaternion& rotation, LevelObject* parent)
    {
        LevelCollection* collection = parent ? parent->GetCollection() : (m_isPlaying ? GetRuntimeCollection() : GetDefaultCollection());
        LevelObject* object = LevelSerializer::Instantiate(*this, variant.guid, collection, parent, {}, false);
        if (!object)
        {
            return nullptr;
        }
        object->GetTransform().SetPositionAndRotation(position, rotation);
        InitializeHierarchy(*object);
        return object;
    }

    void Level::InitializeHierarchy(LevelObject& root)
    {
        InitializeObject(root);
        std::vector<LevelObject*> children = root.GetChildren();
        for (LevelObject* child : children)
        {
            InitializeHierarchy(*child);
        }
    }
}
