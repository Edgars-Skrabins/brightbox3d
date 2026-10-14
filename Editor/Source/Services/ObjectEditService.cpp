#include "Services/ObjectEditService.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "EditorContext.h"
#include "EditorText.h"
#include "Geometry/ModelAsset.h"
#include "Level/LevelFileSchema.h"
#include "Level/LevelSerializer.h"
#include "Rendering/Light.h"
#include "Rendering/MeshRenderer.h"

#include <algorithm>

namespace BrightBox3D::Editor
{
    using Level::LevelCollection;
    using Level::LevelObject;
    using Level::LevelSerializer;
    using Serialization::JsonDocument;
    using Serialization::JsonValue;

    namespace
    {
        constexpr float DirectionalLightIntensity = 2.5f;
        constexpr float LocalLightIntensity = 10.0f;
        constexpr Math::Vector3 DirectionalLightAngles{ 50.0f, -30.0f, 0.0f };
        constexpr Math::Vector3 LocalLightPosition{ 0.0f, 2.0f, 0.0f };
        constexpr Math::Vector3 SpotLightAngles{ 90.0f, 0.0f, 0.0f };

        EditorText LightName(Rendering::LightType type)
        {
            switch (type)
            {
            case Rendering::LightType::Directional:
                return EditorText::DirectionalLight;
            case Rendering::LightType::Point:
                return EditorText::PointLight;
            case Rendering::LightType::Spot:
                return EditorText::SpotLight;
            }
            return EditorText::PointLight;
        }
    }

    void ObjectEditService::Snapshot(const LevelObject& object, JsonDocument& document)
    {
        LevelSerializer::WriteObject(object, document, document.GetAllocator(), true);
    }

    LevelObject* ObjectEditService::RestoreSnapshot(const JsonValue& snapshot, const Core::Guid& parentId, int collectionIndex, int siblingIndex)
    {
        Level::Level* level = m_context.GetLevelService().GetEditLevel();
        if (!level)
        {
            return nullptr;
        }
        Core::Guid id = Core::Guid::Parse(Serialization::GetString(snapshot, Level::LevelFileSchema::Id));
        if (LevelObject* existing = level->FindObject(id))
        {
            level->DestroyObjectImmediate(existing);
        }
        LevelObject* parent = parentId.IsValid() ? level->FindObject(parentId) : nullptr;
        LevelCollection* collection = m_context.GetLevelService().GetCollectionByIndex(collectionIndex);
        LevelObject* restored = LevelSerializer::ReadObject(*level, collection, parent, snapshot, true);
        if (restored)
        {
            restored->SetSiblingIndex(siblingIndex);
            m_context.GetLevelService().MarkDirty(*restored);
        }
        m_context.GetLevelService().BumpLevelRevision();
        return restored;
    }

    LevelObject* ObjectEditService::CreateObject(const Reflection::TypeInfo& type, const std::string& name, LevelObject* parent)
    {
        Level::Level* level = m_context.GetLevelService().GetActiveLevel();
        if (!level)
        {
            return nullptr;
        }
        LevelObject* object = level->CreateObject(type, parent ? parent->GetCollection() : m_context.GetLevelService().GetActiveCollection(), parent);
        if (!object)
        {
            return nullptr;
        }
        object->SetName(name);
        m_context.GetUndoService().RecordCreation({ object }, Text(EditorText::UndoCreate, name));
        m_context.GetSelectionService().Select(object->GetId());
        return object;
    }

    LevelObject* ObjectEditService::CreateModelObject(const Core::Guid& model, const std::string& name, const Math::Vector3& position, LevelObject* parent)
    {
        Level::Level* level = m_context.GetLevelService().GetActiveLevel();
        if (!level)
        {
            return nullptr;
        }
        auto asset = Assets::AssetDatabase::Get().Load<Geometry::ModelAsset>(model);
        if (!asset)
        {
            return nullptr;
        }
        LevelCollection* collection = parent ? parent->GetCollection() : m_context.GetLevelService().GetActiveCollection();
        LevelObject* root = level->CreateObject(LevelObject::StaticType(), collection, parent);
        root->SetName(name);
        root->GetTransform().SetPosition(position);
        const auto& nodes = asset->GetNodes();
        if (nodes.size() <= 1)
        {
            auto* renderer = root->AddComponent<Rendering::MeshRenderer>();
            renderer->SetModel(Assets::AssetReference<Geometry::ModelAsset>(model));
            renderer->SetMeshIndex(nodes.empty() ? 0 : std::max(nodes[0].mesh, 0));
        }
        else
        {
            std::vector<LevelObject*> created(nodes.size(), nullptr);
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                const Geometry::ModelNode& node = nodes[i];
                LevelObject* nodeParent = node.parent >= 0 && created[static_cast<size_t>(node.parent)] ? created[static_cast<size_t>(node.parent)] : root;
                LevelObject* object = level->CreateObject(LevelObject::StaticType(), collection, nodeParent);
                object->SetName(node.name.empty() ? std::string(Text(EditorText::DefaultNodeName)) : node.name);
                object->GetTransform().SetLocalPosition(node.position);
                object->GetTransform().SetLocalRotation({ node.rotation.x, node.rotation.y, node.rotation.z, node.rotation.w });
                object->GetTransform().SetLocalScale(node.scale);
                if (node.mesh >= 0)
                {
                    auto* renderer = object->AddComponent<Rendering::MeshRenderer>();
                    renderer->SetModel(Assets::AssetReference<Geometry::ModelAsset>(model));
                    renderer->SetMeshIndex(node.mesh);
                }
                created[i] = object;
            }
        }
        m_context.GetUndoService().RecordCreation({ root }, Text(EditorText::UndoCreate, name));
        m_context.GetSelectionService().Select(root->GetId());
        return root;
    }

    LevelObject* ObjectEditService::InstantiateLevelObject(const Core::Guid& asset, const Math::Vector3& position, LevelObject* parent)
    {
        Level::Level* level = m_context.GetLevelService().GetActiveLevel();
        if (!level)
        {
            return nullptr;
        }
        LevelObject* object = LevelSerializer::Instantiate(*level, asset, parent ? parent->GetCollection() : m_context.GetLevelService().GetActiveCollection(), parent);
        if (!object)
        {
            return nullptr;
        }
        object->GetTransform().SetPosition(position);
        m_context.GetUndoService().RecordCreation({ object }, Text(EditorText::UndoPlace, object->GetName()));
        m_context.GetSelectionService().Select(object->GetId());
        return object;
    }

    LevelObject* ObjectEditService::CreateLight(Rendering::LightType type)
    {
        Level::Level* level = m_context.GetLevelService().GetActiveLevel();
        if (!level)
        {
            return nullptr;
        }
        LevelObject* object = level->CreateObject(LevelObject::StaticType(), m_context.GetLevelService().GetActiveCollection());
        object->SetName(Text(LightName(type)));
        auto* light = object->AddComponent<Rendering::Light>();
        light->SetLightType(type);
        if (type == Rendering::LightType::Directional)
        {
            light->SetIntensity(DirectionalLightIntensity);
            object->GetTransform().SetLocalEulerAngles(DirectionalLightAngles);
        }
        else
        {
            light->SetIntensity(LocalLightIntensity);
            object->GetTransform().SetLocalPosition(LocalLightPosition);
            if (type == Rendering::LightType::Spot)
            {
                object->GetTransform().SetLocalEulerAngles(SpotLightAngles);
            }
        }
        m_context.GetUndoService().RecordCreation({ object }, Text(EditorText::UndoCreate, object->GetName()));
        m_context.GetSelectionService().Select(object->GetId());
        return object;
    }

    void ObjectEditService::DeleteSelection()
    {
        Level::Level* level = m_context.GetLevelService().GetActiveLevel();
        if (!level)
        {
            return;
        }
        std::vector<LevelObject*> objects = m_context.GetSelectionService().GetSelectedObjects();
        std::vector<LevelObject*> roots;
        for (LevelObject* object : objects)
        {
            bool nested = false;
            for (LevelObject* other : objects)
            {
                if (other != object && object->IsDescendantOf(other))
                {
                    nested = true;
                }
            }
            if (!nested)
            {
                roots.push_back(object);
            }
        }
        if (roots.empty())
        {
            return;
        }
        if (m_context.GetLevelObjectEditService().IsEditingLevelObject())
        {
            roots.erase(std::remove_if(roots.begin(), roots.end(), [](LevelObject* object) { return object->GetParent() == nullptr; }), roots.end());
        }
        m_context.GetUndoService().RecordDeletion(roots);
        for (LevelObject* object : roots)
        {
            if (m_context.GetLevelService().IsPlaying())
            {
                level->DestroyObject(object);
            }
            else
            {
                level->DestroyObjectImmediate(object);
            }
        }
        m_context.GetSelectionService().ClearSelection();
        m_context.GetLevelService().BumpLevelRevision();
    }

    void ObjectEditService::DuplicateSelection()
    {
        Level::Level* level = m_context.GetLevelService().GetActiveLevel();
        if (!level)
        {
            return;
        }
        std::vector<LevelObject*> duplicates;
        for (LevelObject* object : m_context.GetSelectionService().GetSelectedObjects())
        {
            if (m_context.GetLevelObjectEditService().IsEditingLevelObject() && !object->GetParent())
            {
                continue;
            }
            JsonDocument snapshot;
            Snapshot(*object, snapshot);
            LevelObject* copy = LevelSerializer::InstantiateNode(*level, snapshot, object->GetCollection(), object->GetParent(), true);
            if (copy)
            {
                copy->SetName(object->GetName());
                copy->SetSiblingIndex(object->GetSiblingIndex() + 1);
                duplicates.push_back(copy);
            }
        }
        m_context.GetUndoService().RecordCreation(duplicates, Text(EditorText::Duplicate));
        m_context.GetSelectionService().ClearSelection();
        for (LevelObject* copy : duplicates)
        {
            m_context.GetSelectionService().Select(copy->GetId(), true);
        }
        m_context.GetLevelService().BumpLevelRevision();
    }

    void ObjectEditService::Reparent(LevelObject& object, LevelObject* parent, LevelCollection* collection, int siblingIndex)
    {
        if (parent == &object || (parent && parent->IsDescendantOf(&object)))
        {
            return;
        }
        if (object.IsLinked() && !object.GetLink().isRoot && !m_context.GetLevelObjectEditService().IsEditingLevelObject())
        {
            Core::Log::Warning(Text(EditorText::CannotMoveInstanceChild));
            return;
        }
        ObjectLocation from = LocateObject(m_context, object, false);
        m_context.GetLevelService().MarkDirty(object);
        if (parent)
        {
            object.SetParent(parent, true, siblingIndex);
        }
        else
        {
            object.SetParent(nullptr, true);
            m_context.GetLevelService().GetActiveLevel()->SetRootCollection(object, collection ? collection : object.GetCollection(), siblingIndex);
        }
        m_context.GetLevelService().MarkDirty(object);
        m_context.GetUndoService().RecordReparent(object, from);
        m_context.GetLevelService().BumpLevelRevision();
    }
}
