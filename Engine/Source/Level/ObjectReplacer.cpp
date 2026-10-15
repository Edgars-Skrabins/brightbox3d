#include "Level/ObjectReplacer.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Level/MissingComponent.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"

#include <algorithm>
#include <unordered_set>

namespace BrightBox3D::Level
{
    namespace
    {
        const Reflection::ReferenceRemap NoRemap;

        Reflection::ReferenceList SaveReferences(Reflection::EngineObject& object)
        {
            Reflection::ReferenceList references;
            Reflection::ReferenceCarrier carrier(Reflection::ReferenceCarrier::Mode::Save, references, NoRemap);
            object.CarryReferences(carrier);
            return references;
        }

        struct PendingReferences
        {
            Reflection::EngineObject* object = nullptr;
            Reflection::ReferenceList* references = nullptr;
        };

        int Depth(const LevelObject& object)
        {
            int depth = 0;
            for (const LevelObject* parent = object.GetParent(); parent; parent = parent->GetParent())
            {
                ++depth;
            }
            return depth;
        }

        std::string SerializeToString(const Reflection::EngineObject& object)
        {
            Serialization::JsonDocument document;
            Serialization::SerializeProperties(object, document, document.GetAllocator());
            return Serialization::WriteJson(document, Serialization::JsonFormat::Compact);
        }

        void Deserialize(Reflection::EngineObject& object, const std::string& data)
        {
            Serialization::JsonDocument document;
            if (Serialization::ParseJson(data, document))
            {
                Serialization::DeserializeProperties(object, document);
            }
        }

        std::unique_ptr<LevelObjectComponent> CreateComponent(const ReplacedComponent& entry)
        {
            const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(entry.typeName);
            std::unique_ptr<LevelObjectComponent> component;
            if (type && type->IsA(LevelObjectComponent::StaticType()) && !type->IsAbstract())
            {
                component.reset(static_cast<LevelObjectComponent*>(type->CreateInstance()));
                Deserialize(*component, entry.data);
            }
            else
            {
                Core::Log::Warning(Core::Text(Core::EngineText::ReloadComponentMissing, entry.typeName));
                auto missing = std::make_unique<MissingComponent>();
                missing->SetMissing(entry.typeName, entry.data);
                component = std::move(missing);
            }
            Level::SetRuntimeState(*component, entry.state);
            return component;
        }

        std::unique_ptr<LevelObject> CreateObject(const ReplacedObject& record)
        {
            const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(record.typeName);
            bool missing = !type || !type->IsA(LevelObject::StaticType()) || type->IsAbstract();
            std::unique_ptr<Reflection::EngineObject> created((missing ? &LevelObject::StaticType() : type)->CreateInstance());
            std::unique_ptr<LevelObject> object(static_cast<LevelObject*>(created.release()));
            Level::SetRuntimeState(*object, record.state);
            Serialization::CopyProperties(record.transform, object->GetTransform());
            Level::SetRuntimeState(object->GetTransform(), record.transformState);
            if (missing)
            {
                Core::Log::Warning(Core::Text(Core::EngineText::ReloadObjectTypeMissing, record.typeName, record.state.object.name));
                object->SetMissingType(record.typeName, record.brainData);
            }
            else
            {
                Deserialize(*object, record.brainData);
            }
            return object;
        }

        std::unique_ptr<Reflection::EngineObject> CreateEngineObject(const ReplacedEngineObject& record)
        {
            for (const std::string& typeName : record.typeNames)
            {
                const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(typeName);
                if (!type || type->IsAbstract())
                {
                    continue;
                }
                std::unique_ptr<Reflection::EngineObject> object(type->CreateInstance());
                if (typeName == record.typeNames.front())
                {
                    Deserialize(*object, record.data);
                }
                else
                {
                    Core::Log::Warning(Core::Text(Core::EngineText::ReloadTypeFallback, record.typeNames.front(), typeName));
                }
                Level::SetRuntimeState(*object, record.state);
                return object;
            }
            Core::Log::Warning(Core::Text(Core::EngineText::ReloadTypeRemoved, record.typeNames.front()));
            return nullptr;
        }

        void ExtractEngineObjects(Level& level, const ObjectReplacer::Predicate& isReplaced, ReplacementBatch& batch)
        {
            const auto& objects = level.GetEngineObjects();
            for (size_t index = 0; index < objects.size(); ++index)
            {
                Reflection::EngineObject& object = *objects[index];
                if (!isReplaced(object.GetType()))
                {
                    continue;
                }
                ReplacedEngineObject record;
                record.level = &level;
                record.index = index;
                for (const Reflection::TypeInfo* type = &object.GetType(); type; type = type->GetBase())
                {
                    record.typeNames.push_back(type->GetName());
                }
                record.data = SerializeToString(object);
                record.references = SaveReferences(object);
                record.state = Level::GetRuntimeState(object);
                record.oldAddress = &object;
                record.old = level.ReleaseEngineObjectForReplacement(index);
                batch.engineObjects.push_back(std::move(record));
            }
        }
    }

    void ObjectReplacer::Extract(Level& level, const Predicate& isReplaced, ReplacementBatch& batch)
    {
        ExtractEngineObjects(level, isReplaced, batch);
        std::vector<ReplacedObject>& records = batch.objects;
        std::vector<LevelObject*> affected;
        for (const auto& object : level.GetObjects())
        {
            const auto& components = object->GetComponents();
            bool brain = isReplaced(object->GetType());
            bool component = std::any_of(components.begin(), components.end(), [&](const auto& entry) { return isReplaced(entry->GetType()); });
            if (brain || component)
            {
                affected.push_back(object.get());
            }
        }

        size_t first = records.size();
        for (LevelObject* object : affected)
        {
            ReplacedObject record;
            record.level = &level;
            record.id = object->GetId();
            record.parentId = object->GetParent() ? object->GetParent()->GetId() : Core::Guid{};
            record.collection = object->GetCollection();
            record.siblingIndex = object->GetSiblingIndex();
            record.depth = Depth(*object);
            record.replaceBrain = isReplaced(object->GetType());
            record.oldAddress = object;
            records.push_back(std::move(record));
        }

        for (size_t i = first; i < records.size(); ++i)
        {
            ReplacedObject& record = records[i];
            LevelObject* object = level.FindObject(record.id);
            if (!object)
            {
                continue;
            }
            const auto& components = object->GetComponents();
            for (size_t index = 0; index < components.size(); ++index)
            {
                LevelObjectComponent& component = *components[index];
                bool replaced = isReplaced(component.GetType());
                if (!replaced && !record.replaceBrain)
                {
                    continue;
                }
                ReplacedComponent entry;
                entry.typeName = component.GetType().GetName();
                entry.state = Level::GetRuntimeState(component);
                entry.index = index;
                entry.oldAddress = &component;
                if (replaced)
                {
                    entry.data = SerializeToString(component);
                    entry.references = SaveReferences(component);
                }
                record.components.push_back(std::move(entry));
            }

            if (record.replaceBrain)
            {
                record.typeName = object->GetType().GetName();
                record.brainData = SerializeToString(*object);
                record.references = SaveReferences(*object);
                record.state = Level::GetRuntimeState(*object);
                Serialization::CopyProperties(object->GetTransform(), record.transform);
                record.transformState = Level::GetRuntimeState(object->GetTransform());
                std::vector<std::unique_ptr<LevelObjectComponent>> released;
                record.oldBrain = level.ReleaseObjectForReplacement(*object, record.children, released);
                for (size_t index = 0; index < released.size(); ++index)
                {
                    ReplacedComponent& entry = record.components[index];
                    (isReplaced(released[index]->GetType()) ? entry.doomed : entry.kept) = std::move(released[index]);
                }
            }
            else
            {
                for (auto it = record.components.rbegin(); it != record.components.rend(); ++it)
                {
                    it->doomed = level.ReleaseComponentForReplacement(*object, it->index);
                }
            }
        }
    }

    void ObjectReplacer::DestroyExtracted(ReplacementBatch& batch)
    {
        for (ReplacedObject& record : batch.objects)
        {
            for (ReplacedComponent& entry : record.components)
            {
                entry.doomed.reset();
            }
            record.oldBrain.reset();
        }
        for (ReplacedEngineObject& record : batch.engineObjects)
        {
            record.old.reset();
        }
    }

    void ObjectReplacer::Restore(ReplacementBatch& batch)
    {
        Reflection::ReferenceRemap remap;
        std::unordered_set<Level*> levels;
        std::vector<PendingReferences> pending;
        for (ReplacedEngineObject& record : batch.engineObjects)
        {
            std::unique_ptr<Reflection::EngineObject> object = CreateEngineObject(record);
            remap[record.oldAddress] = object.get();
            pending.push_back({ object.get(), &record.references });
            record.level->InstallEngineObjectReplacement(std::move(object), record.index);
            levels.insert(record.level);
        }
        for (Level* level : levels)
        {
            level->RemoveEmptyEngineObjects();
        }

        std::vector<ReplacedObject>& records = batch.objects;
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
                std::unique_ptr<LevelObject> object = CreateObject(record);
                for (ReplacedComponent& entry : record.components)
                {
                    bool recreated = !entry.kept;
                    std::unique_ptr<LevelObjectComponent> component = entry.kept ? std::move(entry.kept) : CreateComponent(entry);
                    if (recreated)
                    {
                        remap[entry.oldAddress] = component.get();
                        pending.push_back({ component.get(), &entry.references });
                    }
                    Level::AttachComponent(*object, std::move(component));
                }
                remap[record.oldAddress] = object.get();
                pending.push_back({ object.get(), &record.references });
                std::vector<LevelObject*> children;
                for (LevelObject* child : record.children)
                {
                    if (!replacedIds.count(child->GetId()))
                    {
                        children.push_back(child);
                    }
                }
                LevelObject* parent = record.parentId.IsValid() ? level.FindObject(record.parentId) : nullptr;
                level.InstallReplacement(std::move(object), children, parent, parent ? parent->GetCollection() : record.collection, record.siblingIndex);
            }
            else if (LevelObject* object = level.FindObject(record.id))
            {
                for (ReplacedComponent& entry : record.components)
                {
                    std::unique_ptr<LevelObjectComponent> component = CreateComponent(entry);
                    remap[entry.oldAddress] = component.get();
                    pending.push_back({ component.get(), &entry.references });
                    level.InstallComponentReplacement(*object, std::move(component), entry.index);
                }
            }
            level.MarkHierarchyChanged();
            levels.insert(&level);
        }
        std::unordered_set<const Reflection::EngineObject*> restored;
        for (const PendingReferences& entry : pending)
        {
            if (entry.object)
            {
                Reflection::ReferenceCarrier carrier(Reflection::ReferenceCarrier::Mode::Restore, *entry.references, remap);
                entry.object->CarryReferences(carrier);
                restored.insert(entry.object);
            }
        }
        for (Level* level : levels)
        {
            level->RemapReferences(remap, restored);
        }
        batch.objects.clear();
        batch.engineObjects.clear();
    }
}
