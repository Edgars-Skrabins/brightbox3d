#include "Level/ObjectReplacer.h"

#include "Core/Log.h"
#include "Level/Level.h"
#include "Level/MissingComponent.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"

#include <algorithm>
#include <unordered_set>

namespace BrightBox3D::Level
{
    namespace
    {
        int Depth(const LevelObjectInstance& object)
        {
            int depth = 0;
            for (const LevelObjectInstance* parent = object.GetParent(); parent; parent = parent->GetParent())
            {
                ++depth;
            }
            return depth;
        }

        std::string SerializeToString(const Reflection::Object& object)
        {
            Serialization::JsonDocument document;
            Serialization::SerializeProperties(object, document, document.GetAllocator());
            return Serialization::WriteJson(document, false);
        }

        void Deserialize(Reflection::Object& object, const std::string& data)
        {
            Serialization::JsonDocument document;
            if (Serialization::ParseJson(data, document))
            {
                Serialization::DeserializeProperties(object, document);
            }
        }

        std::unique_ptr<Component> CreateComponent(const std::string& typeName, const std::string& data)
        {
            const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(typeName);
            if (type && type->IsA(Component::StaticType()) && !type->IsAbstract())
            {
                std::unique_ptr<Component> component(static_cast<Component*>(type->CreateInstance()));
                Deserialize(*component, data);
                return component;
            }
            Core::Log::Warning("Component type '" + typeName + "' no longer exists after reload; its data is preserved");
            auto missing = std::make_unique<MissingComponent>();
            missing->SetMissing(typeName, data);
            return missing;
        }
    }

    void ObjectReplacer::Extract(Level& level, const Predicate& isReplaced, std::vector<ReplacedObject>& records)
    {
        std::vector<LevelObjectInstance*> affected;
        for (const auto& object : level.GetObjects())
        {
            bool brain = isReplaced(object->GetType());
            bool components = std::any_of(object->m_components.begin(), object->m_components.end(), [&](const auto& component) { return isReplaced(component->GetType()); });
            if (brain || components)
            {
                affected.push_back(object.get());
            }
        }

        size_t first = records.size();
        for (LevelObjectInstance* object : affected)
        {
            ReplacedObject record;
            record.level = &level;
            record.id = object->GetId();
            record.parentId = object->m_parent ? object->m_parent->GetId() : Core::Guid{};
            record.collection = object->m_collection;
            record.siblingIndex = object->GetSiblingIndex();
            record.depth = Depth(*object);
            record.replaceBrain = isReplaced(object->GetType());
            record.componentCount = object->m_components.size();
            records.push_back(std::move(record));
        }

        for (size_t i = first; i < records.size(); ++i)
        {
            ReplacedObject& record = records[i];
            LevelObjectInstance* object = level.FindObject(record.id);
            if (!object)
            {
                continue;
            }
            for (size_t index = 0; index < object->m_components.size(); ++index)
            {
                Component& component = *object->m_components[index];
                if (!isReplaced(component.GetType()) && !record.replaceBrain)
                {
                    continue;
                }
                ReplacedComponent entry;
                entry.typeName = component.GetType().GetName();
                entry.localId = component.m_localId;
                entry.enabled = component.m_isEnabled;
                entry.hasLoaded = component.m_hasLoaded;
                entry.hasStarted = component.m_hasStarted;
                entry.enabledState = component.m_isEnabledState;
                entry.index = index;
                if (isReplaced(component.GetType()))
                {
                    entry.data = SerializeToString(component);
                }
                record.components.push_back(std::move(entry));
            }

            if (record.replaceBrain)
            {
                record.typeName = object->GetType().GetName();
                record.brainData = SerializeToString(*object);
                record.name = object->m_name;
                record.active = object->m_isActive;
                record.link = object->m_link;
                record.ownerId = object->m_ownerId;
                record.hideInLevelContents = object->m_hideInLevelContents;
                record.hasLoaded = object->m_hasLoaded;
                record.hasStarted = object->m_hasStarted;
                record.enabledState = object->m_isEnabledState;
                record.pendingDestroy = object->m_isPendingDestroy;
                record.localPosition = object->m_transform.GetLocalPosition();
                record.localRotation = object->m_transform.GetLocalRotation();
                record.localScale = object->m_transform.GetLocalScale();
                record.transformLocalId = object->m_transform.m_localId;
                record.children = object->m_children;
                record.oldBrain = level.ReleaseObjectForReplacement(*object);
                object->m_children.clear();
                size_t entryIndex = 0;
                for (size_t index = 0; index < object->m_components.size(); ++index)
                {
                    ReplacedComponent& entry = record.components[entryIndex++];
                    if (isReplaced(object->m_components[index]->GetType()))
                    {
                        entry.doomed = std::move(object->m_components[index]);
                    }
                    else
                    {
                        entry.kept = std::move(object->m_components[index]);
                    }
                }
                object->m_components.clear();
            }
            else
            {
                for (ReplacedComponent& entry : record.components)
                {
                    Component* component = object->m_components[entry.index].get();
                    for (const auto& subsystem : level.GetSubsystems())
                    {
                        subsystem->OnComponentRemoved(*component);
                    }
                }
                for (auto it = record.components.rbegin(); it != record.components.rend(); ++it)
                {
                    it->doomed = std::move(object->m_components[it->index]);
                    object->m_components.erase(object->m_components.begin() + static_cast<std::ptrdiff_t>(it->index));
                }
            }
        }
    }

    void ObjectReplacer::DestroyExtracted(std::vector<ReplacedObject>& records)
    {
        for (ReplacedObject& record : records)
        {
            for (ReplacedComponent& entry : record.components)
            {
                entry.doomed.reset();
            }
            record.oldBrain.reset();
        }
    }

    void ObjectReplacer::Restore(std::vector<ReplacedObject>& records)
    {
        std::stable_sort(records.begin(), records.end(), [](const ReplacedObject& a, const ReplacedObject& b) { return a.depth < b.depth; });
        std::unordered_set<Core::Guid> replacedIds;
        for (const ReplacedObject& record : records)
        {
            if (record.replaceBrain)
            {
                replacedIds.insert(record.id);
            }
        }

        for (ReplacedObject& record : records)
        {
            Level& level = *record.level;
            if (record.replaceBrain)
            {
                const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(record.typeName);
                bool missing = !type || !type->IsA(LevelObjectInstance::StaticType()) || type->IsAbstract();
                std::unique_ptr<Reflection::Object> created((missing ? &LevelObjectInstance::StaticType() : type)->CreateInstance());
                std::unique_ptr<LevelObjectInstance> object(static_cast<LevelObjectInstance*>(created.release()));
                object->m_id = record.id;
                object->m_name = record.name;
                object->m_isActive = record.active;
                object->m_link = record.link;
                object->m_ownerId = record.ownerId;
                object->m_hideInLevelContents = record.hideInLevelContents;
                object->m_hasLoaded = record.hasLoaded;
                object->m_hasStarted = record.hasStarted;
                object->m_isEnabledState = record.enabledState;
                object->m_isPendingDestroy = record.pendingDestroy;
                object->m_transform.m_localId = record.transformLocalId;
                object->m_transform.SetLocalPosition(record.localPosition);
                object->m_transform.SetLocalRotation(record.localRotation);
                object->m_transform.SetLocalScale(record.localScale);
                if (missing)
                {
                    Core::Log::Warning("Type '" + record.typeName + "' no longer exists after reload; '" + record.name + "' keeps its data as a missing type");
                    object->SetMissingType(record.typeName, record.brainData);
                }
                else
                {
                    Deserialize(*object, record.brainData);
                }
                for (ReplacedComponent& entry : record.components)
                {
                    std::unique_ptr<Component> component = entry.kept ? std::move(entry.kept) : CreateComponent(entry.typeName, entry.data);
                    component->m_localId = entry.localId;
                    component->m_isEnabled = entry.enabled;
                    component->m_hasLoaded = entry.hasLoaded;
                    component->m_hasStarted = entry.hasStarted;
                    component->m_isEnabledState = entry.enabledState;
                    component->m_owner = object.get();
                    object->m_components.push_back(std::move(component));
                }
                for (LevelObjectInstance* child : record.children)
                {
                    if (!replacedIds.count(child->GetId()))
                    {
                        object->m_children.push_back(child);
                    }
                }
                LevelObjectInstance* parent = record.parentId.IsValid() ? level.FindObject(record.parentId) : nullptr;
                level.InstallReplacement(std::move(object), parent, parent ? parent->m_collection : record.collection, record.siblingIndex);
            }
            else
            {
                LevelObjectInstance* object = level.FindObject(record.id);
                if (!object)
                {
                    continue;
                }
                for (ReplacedComponent& entry : record.components)
                {
                    std::unique_ptr<Component> component = CreateComponent(entry.typeName, entry.data);
                    component->m_localId = entry.localId;
                    component->m_isEnabled = entry.enabled;
                    component->m_hasLoaded = entry.hasLoaded;
                    component->m_hasStarted = entry.hasStarted;
                    component->m_isEnabledState = entry.enabledState;
                    component->m_owner = object;
                    Component* raw = component.get();
                    size_t index = std::min(entry.index, object->m_components.size());
                    object->m_components.insert(object->m_components.begin() + static_cast<std::ptrdiff_t>(index), std::move(component));
                    for (const auto& subsystem : level.GetSubsystems())
                    {
                        subsystem->OnComponentAdded(*raw);
                    }
                }
            }
            level.MarkHierarchyChanged();
        }
        records.clear();
    }
}
