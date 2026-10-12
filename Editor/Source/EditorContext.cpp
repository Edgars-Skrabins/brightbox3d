#include "EditorContext.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "Level/LevelAssets.h"
#include "Level/LevelSerializer.h"
#include "PlaySession.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/Light.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/ModelAsset.h"
#include "Serialization/ObjectSerializer.h"

#include <algorithm>

namespace BrightBox3D::Editor
{
    using Level::LevelCollection;
    using Level::LevelObjectInstance;
    using Level::LevelSerializer;
    using Serialization::JsonDocument;
    using Serialization::JsonValue;

    namespace
    {
        struct ObjectLocation
        {
            Core::Guid id;
            Core::Guid parent;
            int collection = 0;
            int sibling = 0;
            std::shared_ptr<JsonDocument> snapshot;
        };

        class ObjectChangeCommand : public EditorCommand
        {
        public:
            ObjectChangeCommand(std::string name, ObjectLocation location, std::shared_ptr<JsonDocument> before, std::shared_ptr<JsonDocument> after)
                : m_name(std::move(name)), m_location(std::move(location)), m_before(std::move(before)), m_after(std::move(after))
            {
            }

            void Undo(EditorContext& context) override { Apply(context, *m_before); }
            void Redo(EditorContext& context) override { Apply(context, *m_after); }
            std::string GetName() const override { return m_name; }

        private:
            void Apply(EditorContext& context, const JsonValue& state)
            {
                Level::Level* level = context.GetEditLevel();
                LevelObjectInstance* existing = level ? level->FindObject(m_location.id) : nullptr;
                Core::Guid parent = m_location.parent;
                int collection = m_location.collection;
                int sibling = m_location.sibling;
                if (existing)
                {
                    parent = existing->GetParent() ? existing->GetParent()->GetId() : Core::Guid{};
                    collection = context.GetCollectionIndex(existing->GetCollection());
                    sibling = existing->GetSiblingIndex();
                }
                context.RestoreSnapshot(state, parent, collection, sibling);
            }

            std::string m_name;
            ObjectLocation m_location;
            std::shared_ptr<JsonDocument> m_before;
            std::shared_ptr<JsonDocument> m_after;
        };

        class ExistenceCommand : public EditorCommand
        {
        public:
            ExistenceCommand(std::string name, std::vector<ObjectLocation> objects, bool created) : m_name(std::move(name)), m_objects(std::move(objects)), m_created(created) {}

            void Undo(EditorContext& context) override
            {
                m_created ? Remove(context) : Restore(context);
            }

            void Redo(EditorContext& context) override
            {
                m_created ? Restore(context) : Remove(context);
            }

            std::string GetName() const override { return m_name; }

        private:
            void Remove(EditorContext& context)
            {
                Level::Level* level = context.GetEditLevel();
                for (auto it = m_objects.rbegin(); it != m_objects.rend(); ++it)
                {
                    if (LevelObjectInstance* object = level->FindObject(it->id))
                    {
                        context.MarkDirty(*object);
                        level->DestroyObjectImmediate(object);
                    }
                }
                context.ClearSelection();
                context.BumpLevelRevision();
            }

            void Restore(EditorContext& context)
            {
                context.ClearSelection();
                for (const ObjectLocation& location : m_objects)
                {
                    if (LevelObjectInstance* object = context.RestoreSnapshot(*location.snapshot, location.parent, location.collection, location.sibling))
                    {
                        context.Select(object->GetId(), true);
                    }
                }
                context.BumpLevelRevision();
            }

            std::string m_name;
            std::vector<ObjectLocation> m_objects;
            bool m_created;
        };

        class ReparentCommand : public EditorCommand
        {
        public:
            ReparentCommand(const Core::Guid& id, ObjectLocation from, ObjectLocation to) : m_id(id), m_from(std::move(from)), m_to(std::move(to)) {}

            void Undo(EditorContext& context) override { Apply(context, m_from); }
            void Redo(EditorContext& context) override { Apply(context, m_to); }
            std::string GetName() const override { return "Reparent"; }

        private:
            void Apply(EditorContext& context, const ObjectLocation& location)
            {
                Level::Level* level = context.GetEditLevel();
                LevelObjectInstance* object = level->FindObject(m_id);
                if (!object)
                {
                    return;
                }
                context.MarkDirty(*object);
                LevelObjectInstance* parent = location.parent.IsValid() ? level->FindObject(location.parent) : nullptr;
                if (parent)
                {
                    object->SetParent(parent, true, location.sibling);
                }
                else
                {
                    object->SetParent(nullptr, true);
                    level->SetRootCollection(*object, context.GetCollectionByIndex(location.collection), location.sibling);
                }
                context.MarkDirty(*object);
                context.BumpLevelRevision();
            }

            Core::Guid m_id;
            ObjectLocation m_from;
            ObjectLocation m_to;
        };

        ObjectLocation Locate(EditorContext& context, const LevelObjectInstance& object, bool withSnapshot)
        {
            ObjectLocation location;
            location.id = object.GetId();
            location.parent = object.GetParent() ? object.GetParent()->GetId() : Core::Guid{};
            location.collection = context.GetCollectionIndex(object.GetCollection());
            location.sibling = object.GetSiblingIndex();
            if (withSnapshot)
            {
                location.snapshot = std::make_shared<JsonDocument>();
                EditorContext::Snapshot(object, *location.snapshot);
            }
            return location;
        }
    }

    EditorContext::EditorContext()
    {
        m_assetChangedHandle = Assets::AssetDatabase::Get().OnAssetChanged.Subscribe([this](const Core::Guid& guid) { OnAssetChanged(guid); });
    }

    EditorContext::~EditorContext()
    {
        Assets::AssetDatabase::Get().OnAssetChanged.Unsubscribe(m_assetChangedHandle);
    }

    bool EditorContext::OpenProject(const std::filesystem::path& directory, std::string& error)
    {
        CloseProject();
        if (!m_project.Open(directory, error))
        {
            return false;
        }
        Runtime::Project::SetCurrent(&m_project);
        Assets::AssetDatabase::Get().Open(m_project.GetRoot());
        if (m_project.GetStartupLevel().IsValid() && Assets::AssetDatabase::Get().FindRecord(m_project.GetStartupLevel()))
        {
            OpenLevel(m_project.GetStartupLevel());
        }
        else
        {
            auto levels = Assets::AssetDatabase::Get().GetRecords(Assets::AssetType::Level);
            if (!levels.empty())
            {
                OpenLevel(levels.front().guid);
            }
            else
            {
                NewLevel("Levels/Main.level");
                m_project.SetStartupLevel(m_level ? m_level->GetAsset() : Core::Guid{});
                m_project.Save();
            }
        }
        return true;
    }

    void EditorContext::CloseProject()
    {
        m_level.reset();
        m_stashedLevel.reset();
        m_editedLevelObject = {};
        ResetSelection();
        m_undo.Clear();
        Level::LevelObjectLibrary::Get().Clear();
        Assets::AssetDatabase::Get().Close();
        Runtime::Project::SetCurrent(nullptr);
        m_project = Runtime::Project();
    }

    Level::Level* EditorContext::GetActiveLevel() const
    {
        if (m_playSession && m_playSession->IsActive() && m_playSession->GetLevel())
        {
            return m_playSession->GetLevel();
        }
        return m_level.get();
    }

    bool EditorContext::IsPlaying() const
    {
        return m_playSession && m_playSession->IsActive();
    }

    void EditorContext::ResetSelection()
    {
        m_selection.clear();
        m_selectedCollection = nullptr;
        m_activeCollection = nullptr;
        m_focus = SelectionFocus::None;
    }

    bool EditorContext::OpenLevel(const Core::Guid& levelAsset)
    {
        if (IsEditingLevelObject())
        {
            CloseLevelObjectEditor(true);
        }
        std::string error;
        std::unique_ptr<Level::Level> level = LevelSerializer::LoadLevel(levelAsset, &error);
        if (!level)
        {
            Core::Log::Error("Failed to open level: " + error);
            return false;
        }
        level->SetFixedDeltaTime(m_project.GetFixedTimeStep());
        m_level = std::move(level);
        ResetSelection();
        m_undo.Clear();
        ++m_levelRevision;
        Core::Log::Info("Opened level '" + m_level->GetName() + "'");
        return true;
    }

    bool EditorContext::NewLevel(const std::string& relativePath)
    {
        std::string error;
        Core::Guid guid = LevelSerializer::CreateLevelAsset(relativePath, &error);
        if (!guid.IsValid())
        {
            Core::Log::Error("Failed to create level: " + error);
            return false;
        }
        if (!OpenLevel(guid))
        {
            return false;
        }
        LevelObjectInstance* sun = CreateLight(0);
        if (sun)
        {
            sun->SetName("Directional Light");
            sun->GetTransform().SetLocalEulerAngles({ 50.0f, -30.0f, 0.0f });
            sun->GetTransform().SetLocalPosition({ 0.0f, 5.0f, 0.0f });
        }
        CreateModelObject(Rendering::BuiltinAssets::Plane(), "Ground", Math::Vector3::Zero());
        m_undo.Clear();
        ClearSelection();
        SaveLevel();
        return true;
    }

    bool EditorContext::SaveLevel()
    {
        if (IsPlaying())
        {
            Core::Log::Warning("Stop PlayMode before saving");
            return false;
        }
        if (IsEditingLevelObject())
        {
            return SaveLevelObjectEditor();
        }
        if (!m_level)
        {
            return false;
        }
        bool saved = LevelSerializer::SaveLevel(*m_level, false);
        if (saved)
        {
            Core::Log::Info("Saved level '" + m_level->GetName() + "'");
        }
        else
        {
            Core::Log::Error("Saving level '" + m_level->GetName() + "' failed");
        }
        return saved;
    }

    bool EditorContext::IsDirty() const
    {
        if (IsEditingLevelObject())
        {
            return m_levelObjectDirty;
        }
        if (!m_level)
        {
            return false;
        }
        for (const auto& collection : m_level->GetCollections())
        {
            if (collection->IsDirty())
            {
                return true;
            }
        }
        return false;
    }

    void EditorContext::MarkDirty(LevelCollection* collection)
    {
        if (IsEditingLevelObject())
        {
            m_levelObjectDirty = true;
        }
        if (collection)
        {
            collection->SetDirty(true);
        }
    }

    void EditorContext::MarkDirty(const LevelObjectInstance& object)
    {
        if (object.GetLevel() == m_level.get())
        {
            MarkDirty(object.GetCollection());
        }
    }

    LevelCollection* EditorContext::GetActiveCollection() const
    {
        Level::Level* level = GetActiveLevel();
        if (!level)
        {
            return nullptr;
        }
        if (m_activeCollection && GetCollectionIndex(m_activeCollection) >= 0)
        {
            return m_activeCollection;
        }
        return level->GetDefaultCollection();
    }

    void EditorContext::SetActiveCollection(LevelCollection* collection)
    {
        m_activeCollection = collection;
    }

    int EditorContext::GetCollectionIndex(const LevelCollection* collection) const
    {
        Level::Level* level = GetActiveLevel();
        if (!level || !collection)
        {
            return -1;
        }
        const auto& collections = level->GetCollections();
        for (size_t i = 0; i < collections.size(); ++i)
        {
            if (collections[i].get() == collection)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    LevelCollection* EditorContext::GetCollectionByIndex(int index) const
    {
        Level::Level* level = GetActiveLevel();
        if (!level || index < 0 || index >= static_cast<int>(level->GetCollections().size()))
        {
            return level ? level->GetDefaultCollection() : nullptr;
        }
        return level->GetCollections()[static_cast<size_t>(index)].get();
    }

    bool EditorContext::OpenLevelObjectEditor(const Core::Guid& asset)
    {
        if (IsPlaying())
        {
            return false;
        }
        auto loaded = Assets::AssetDatabase::Get().LoadAsset(asset);
        const JsonValue* root = nullptr;
        if (auto* levelObject = loaded ? Reflection::Cast<Level::LevelObjectAsset>(loaded.get()) : nullptr)
        {
            root = levelObject->GetRoot();
        }
        else if (auto* templateAsset = loaded ? Reflection::Cast<Level::LevelObjectTemplateAsset>(loaded.get()) : nullptr)
        {
            root = templateAsset->GetRoot();
        }
        if (!root)
        {
            Core::Log::Error("Cannot open asset for editing");
            return false;
        }
        if (IsEditingLevelObject())
        {
            CloseLevelObjectEditor(true);
        }
        auto level = std::make_unique<Level::Level>(loaded->GetName());
        LevelCollection* collection = level->AddCollection(loaded->GetName());
        LevelObjectInstance* object = LevelSerializer::ReadObject(*level, collection, nullptr, *root, true);
        if (!object)
        {
            return false;
        }
        std::function<void(LevelObjectInstance&)> tag = [&](LevelObjectInstance& node)
        {
            Level::LevelObjectLink link;
            link.sourceId = node.GetId();
            node.SetLink(link);
            for (LevelObjectInstance* child : node.GetChildren())
            {
                tag(*child);
            }
        };
        tag(*object);

        m_stashedLevel = std::move(m_level);
        m_stashedSelection = m_selection;
        m_stashedUndo = std::move(m_undo);
        m_undo = UndoStack();
        m_level = std::move(level);
        m_editedLevelObject = asset;
        m_levelObjectDirty = false;
        ResetSelection();
        Select(object->GetId());
        ++m_levelRevision;
        return true;
    }

    bool EditorContext::SaveLevelObjectEditor()
    {
        if (!IsEditingLevelObject() || !m_level || m_level->GetCollections().empty() || m_level->GetCollections().front()->GetRoots().empty())
        {
            return false;
        }
        LevelObjectInstance* root = m_level->GetCollections().front()->GetRoots().front();
        JsonDocument document;
        LevelSerializer::MakeLevelObjectDocument(*root, document);
        const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(m_editedLevelObject);
        if (record && record->type == Assets::AssetType::LevelObjectTemplate)
        {
            document["$asset"].SetString("LevelObjectTemplate");
        }
        bool saved = Assets::AssetDatabase::Get().SaveJsonAsset(m_editedLevelObject, document);
        if (saved)
        {
            m_levelObjectDirty = false;
            Core::Log::Info("Saved '" + m_level->GetName() + "'");
        }
        return saved;
    }

    void EditorContext::CloseLevelObjectEditor(bool save)
    {
        if (!IsEditingLevelObject())
        {
            return;
        }
        if (save && m_levelObjectDirty)
        {
            SaveLevelObjectEditor();
        }
        m_level = std::move(m_stashedLevel);
        m_undo = std::move(m_stashedUndo);
        m_editedLevelObject = {};
        ResetSelection();
        for (const Core::Guid& id : m_stashedSelection)
        {
            if (m_level && m_level->FindObject(id))
            {
                m_selection.push_back(id);
                m_focus = SelectionFocus::Objects;
            }
        }
        ++m_levelRevision;
    }

    void EditorContext::OnAssetChanged(const Core::Guid& guid)
    {
        const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(guid);
        if (!record || record->type != Assets::AssetType::LevelObject)
        {
            return;
        }
        Level::Level* target = IsEditingLevelObject() ? m_stashedLevel.get() : m_level.get();
        if (target)
        {
            LevelSerializer::RefreshLinkedInstances(*target, guid);
            ++m_levelRevision;
        }
        else
        {
            Level::LevelObjectLibrary::Get().Invalidate(guid);
        }
    }

    LevelObjectInstance* EditorContext::GetPrimarySelection() const
    {
        Level::Level* level = GetActiveLevel();
        if (!level || m_selection.empty() || m_focus != SelectionFocus::Objects)
        {
            return nullptr;
        }
        return level->FindObject(m_selection.back());
    }

    std::vector<LevelObjectInstance*> EditorContext::GetSelectedObjects() const
    {
        std::vector<LevelObjectInstance*> objects;
        Level::Level* level = GetActiveLevel();
        if (!level)
        {
            return objects;
        }
        for (const Core::Guid& id : m_selection)
        {
            if (LevelObjectInstance* object = level->FindObject(id))
            {
                objects.push_back(object);
            }
        }
        return objects;
    }

    void EditorContext::Select(const Core::Guid& id, bool additive)
    {
        if (!additive)
        {
            m_selection.clear();
        }
        auto it = std::find(m_selection.begin(), m_selection.end(), id);
        if (it != m_selection.end())
        {
            if (additive)
            {
                m_selection.erase(it);
            }
            else
            {
                m_selection.push_back(id);
            }
        }
        else
        {
            m_selection.push_back(id);
        }
        m_focus = m_selection.empty() ? SelectionFocus::None : SelectionFocus::Objects;
    }

    void EditorContext::ClearSelection()
    {
        m_selection.clear();
        if (m_focus == SelectionFocus::Objects)
        {
            m_focus = SelectionFocus::None;
        }
    }

    bool EditorContext::IsSelected(const Core::Guid& id) const
    {
        return std::find(m_selection.begin(), m_selection.end(), id) != m_selection.end();
    }

    void EditorContext::SelectAsset(const Core::Guid& asset)
    {
        m_selectedAsset = asset;
        m_focus = asset.IsValid() ? SelectionFocus::Asset : SelectionFocus::None;
    }

    void EditorContext::SelectCollection(LevelCollection* collection)
    {
        m_selectedCollection = collection;
        m_selection.clear();
        m_focus = collection ? SelectionFocus::Collection : SelectionFocus::None;
    }

    void EditorContext::SelectLevel()
    {
        m_selection.clear();
        m_focus = SelectionFocus::Level;
    }

    void EditorContext::Undo()
    {
        if (!IsPlaying())
        {
            m_undo.Undo(*this);
            ++m_levelRevision;
        }
    }

    void EditorContext::Redo()
    {
        if (!IsPlaying())
        {
            m_undo.Redo(*this);
            ++m_levelRevision;
        }
    }

    void EditorContext::Snapshot(const LevelObjectInstance& object, JsonDocument& document)
    {
        LevelSerializer::WriteObject(object, document, document.GetAllocator(), true);
    }

    LevelObjectInstance* EditorContext::RestoreSnapshot(const JsonValue& snapshot, const Core::Guid& parentId, int collectionIndex, int siblingIndex)
    {
        Level::Level* level = m_level.get();
        if (!level)
        {
            return nullptr;
        }
        Core::Guid id = Core::Guid::Parse(Serialization::GetString(snapshot, "id"));
        if (LevelObjectInstance* existing = level->FindObject(id))
        {
            level->DestroyObjectImmediate(existing);
        }
        LevelObjectInstance* parent = parentId.IsValid() ? level->FindObject(parentId) : nullptr;
        LevelCollection* collection = GetCollectionByIndex(collectionIndex);
        LevelObjectInstance* restored = LevelSerializer::ReadObject(*level, collection, parent, snapshot, true);
        if (restored)
        {
            restored->SetSiblingIndex(siblingIndex);
            MarkDirty(*restored);
        }
        ++m_levelRevision;
        return restored;
    }

    void EditorContext::RecordCreation(const std::vector<LevelObjectInstance*>& objects, const std::string& name)
    {
        if (IsPlaying() || objects.empty())
        {
            return;
        }
        std::vector<ObjectLocation> locations;
        for (LevelObjectInstance* object : objects)
        {
            locations.push_back(Locate(*this, *object, true));
            MarkDirty(*object);
        }
        m_undo.Push(std::make_unique<ExistenceCommand>(name, std::move(locations), true));
        ++m_levelRevision;
    }

    void EditorContext::RecordObjectChange(const Core::Guid& id, const JsonValue& before, const std::string& name)
    {
        if (IsPlaying() || !m_level)
        {
            return;
        }
        LevelObjectInstance* object = m_level->FindObject(id);
        if (!object)
        {
            return;
        }
        auto beforeDocument = std::make_shared<JsonDocument>();
        beforeDocument->CopyFrom(before, beforeDocument->GetAllocator());
        auto afterDocument = std::make_shared<JsonDocument>();
        Snapshot(*object, *afterDocument);
        if (*beforeDocument == *afterDocument)
        {
            return;
        }
        m_undo.Push(std::make_unique<ObjectChangeCommand>(name, Locate(*this, *object, false), beforeDocument, afterDocument));
        MarkDirty(*object);
    }

    LevelObjectInstance* EditorContext::CreateObject(const Reflection::TypeInfo& type, const std::string& name, LevelObjectInstance* parent)
    {
        Level::Level* level = GetActiveLevel();
        if (!level)
        {
            return nullptr;
        }
        LevelObjectInstance* object = level->CreateObject(type, parent ? parent->GetCollection() : GetActiveCollection(), parent);
        if (!object)
        {
            return nullptr;
        }
        object->SetName(name);
        RecordCreation({ object }, "Create " + name);
        Select(object->GetId());
        return object;
    }

    LevelObjectInstance* EditorContext::CreateModelObject(const Core::Guid& model, const std::string& name, const Math::Vector3& position, LevelObjectInstance* parent)
    {
        Level::Level* level = GetActiveLevel();
        if (!level)
        {
            return nullptr;
        }
        auto asset = Assets::AssetDatabase::Get().Load<Rendering::ModelAsset>(model);
        if (!asset)
        {
            return nullptr;
        }
        LevelCollection* collection = parent ? parent->GetCollection() : GetActiveCollection();
        LevelObjectInstance* root = level->CreateObject(LevelObjectInstance::StaticType(), collection, parent);
        root->SetName(name);
        root->GetTransform().SetPosition(position);
        const auto& nodes = asset->GetNodes();
        if (nodes.size() <= 1)
        {
            auto* renderer = root->AddComponent<Rendering::MeshRenderer>();
            renderer->SetModel(Assets::AssetReference<Rendering::ModelAsset>(model));
            renderer->SetMeshIndex(nodes.empty() ? 0 : std::max(nodes[0].mesh, 0));
        }
        else
        {
            std::vector<LevelObjectInstance*> created(nodes.size(), nullptr);
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                const Rendering::ModelNode& node = nodes[i];
                LevelObjectInstance* nodeParent = node.parent >= 0 && created[static_cast<size_t>(node.parent)] ? created[static_cast<size_t>(node.parent)] : root;
                LevelObjectInstance* object = level->CreateObject(LevelObjectInstance::StaticType(), collection, nodeParent);
                object->SetName(node.name.empty() ? "Node" : node.name);
                object->GetTransform().SetLocalPosition(node.position);
                object->GetTransform().SetLocalRotation({ node.rotation.x, node.rotation.y, node.rotation.z, node.rotation.w });
                object->GetTransform().SetLocalScale(node.scale);
                if (node.mesh >= 0)
                {
                    auto* renderer = object->AddComponent<Rendering::MeshRenderer>();
                    renderer->SetModel(Assets::AssetReference<Rendering::ModelAsset>(model));
                    renderer->SetMeshIndex(node.mesh);
                }
                created[i] = object;
            }
        }
        RecordCreation({ root }, "Create " + name);
        Select(root->GetId());
        return root;
    }

    LevelObjectInstance* EditorContext::InstantiateLevelObject(const Core::Guid& asset, const Math::Vector3& position, LevelObjectInstance* parent)
    {
        Level::Level* level = GetActiveLevel();
        if (!level)
        {
            return nullptr;
        }
        LevelObjectInstance* object = LevelSerializer::Instantiate(*level, asset, parent ? parent->GetCollection() : GetActiveCollection(), parent);
        if (!object)
        {
            return nullptr;
        }
        object->GetTransform().SetPosition(position);
        RecordCreation({ object }, "Place " + object->GetName());
        Select(object->GetId());
        return object;
    }

    LevelObjectInstance* EditorContext::CreateLight(int lightType)
    {
        static const char* names[] = { "Directional Light", "Point Light", "Spot Light" };
        Level::Level* level = GetActiveLevel();
        if (!level)
        {
            return nullptr;
        }
        LevelObjectInstance* object = level->CreateObject(LevelObjectInstance::StaticType(), GetActiveCollection());
        object->SetName(names[std::clamp(lightType, 0, 2)]);
        auto* light = object->AddComponent<Rendering::Light>();
        light->SetLightType(static_cast<Rendering::LightType>(std::clamp(lightType, 0, 2)));
        if (lightType == 0)
        {
            light->SetIntensity(2.5f);
            object->GetTransform().SetLocalEulerAngles({ 50.0f, -30.0f, 0.0f });
        }
        else
        {
            light->SetIntensity(10.0f);
            object->GetTransform().SetLocalPosition({ 0.0f, 2.0f, 0.0f });
            if (lightType == 2)
            {
                object->GetTransform().SetLocalEulerAngles({ 90.0f, 0.0f, 0.0f });
            }
        }
        RecordCreation({ object }, "Create " + object->GetName());
        Select(object->GetId());
        return object;
    }

    void EditorContext::DeleteSelection()
    {
        Level::Level* level = GetActiveLevel();
        if (!level)
        {
            return;
        }
        std::vector<LevelObjectInstance*> objects = GetSelectedObjects();
        std::vector<LevelObjectInstance*> roots;
        for (LevelObjectInstance* object : objects)
        {
            bool nested = false;
            for (LevelObjectInstance* other : objects)
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
        if (IsEditingLevelObject())
        {
            roots.erase(std::remove_if(roots.begin(), roots.end(), [](LevelObjectInstance* object) { return object->GetParent() == nullptr; }), roots.end());
        }
        if (!IsPlaying())
        {
            std::vector<ObjectLocation> locations;
            for (LevelObjectInstance* object : roots)
            {
                locations.push_back(Locate(*this, *object, true));
                MarkDirty(*object);
            }
            m_undo.Push(std::make_unique<ExistenceCommand>("Delete", std::move(locations), false));
        }
        for (LevelObjectInstance* object : roots)
        {
            if (IsPlaying())
            {
                level->DestroyObject(object);
            }
            else
            {
                level->DestroyObjectImmediate(object);
            }
        }
        ClearSelection();
        ++m_levelRevision;
    }

    void EditorContext::DuplicateSelection()
    {
        Level::Level* level = GetActiveLevel();
        if (!level)
        {
            return;
        }
        std::vector<LevelObjectInstance*> duplicates;
        for (LevelObjectInstance* object : GetSelectedObjects())
        {
            if (IsEditingLevelObject() && !object->GetParent())
            {
                continue;
            }
            JsonDocument snapshot;
            Snapshot(*object, snapshot);
            LevelObjectInstance* copy = LevelSerializer::InstantiateNode(*level, snapshot, object->GetCollection(), object->GetParent(), true);
            if (copy)
            {
                copy->SetName(object->GetName());
                copy->SetSiblingIndex(object->GetSiblingIndex() + 1);
                duplicates.push_back(copy);
            }
        }
        RecordCreation(duplicates, "Duplicate");
        ClearSelection();
        for (LevelObjectInstance* copy : duplicates)
        {
            Select(copy->GetId(), true);
        }
        ++m_levelRevision;
    }

    void EditorContext::Reparent(LevelObjectInstance& object, LevelObjectInstance* parent, LevelCollection* collection, int siblingIndex)
    {
        if (parent == &object || (parent && parent->IsDescendantOf(&object)))
        {
            return;
        }
        if (object.IsLinked() && !object.GetLink().isRoot && !IsEditingLevelObject())
        {
            Core::Log::Warning("Objects inside a LevelObject instance cannot be moved; unlink the instance or edit the LevelObject");
            return;
        }
        ObjectLocation from = Locate(*this, object, false);
        MarkDirty(object);
        if (parent)
        {
            object.SetParent(parent, true, siblingIndex);
        }
        else
        {
            object.SetParent(nullptr, true);
            GetActiveLevel()->SetRootCollection(object, collection ? collection : object.GetCollection(), siblingIndex);
        }
        MarkDirty(object);
        ObjectLocation to = Locate(*this, object, false);
        if (!IsPlaying())
        {
            m_undo.Push(std::make_unique<ReparentCommand>(object.GetId(), from, to));
        }
        ++m_levelRevision;
    }

    Core::Guid EditorContext::CreateLevelObjectAsset(LevelObjectInstance& object, const std::string& folder)
    {
        if (IsPlaying())
        {
            return {};
        }
        JsonDocument before;
        Snapshot(object, before);
        JsonDocument document;
        LevelSerializer::MakeLevelObjectDocument(object, document);
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        std::string path = database.MakeUniquePath((folder.empty() ? "" : folder + "/") + object.GetName() + ".levelobject");
        Core::Guid asset = database.CreateJsonAsset(path, document);
        if (!asset.IsValid())
        {
            return {};
        }
        Core::Guid id = object.GetId();
        std::function<void(LevelObjectInstance&, bool)> link = [&](LevelObjectInstance& node, bool isRoot)
        {
            Level::LevelObjectLink info;
            info.asset = isRoot ? asset : Core::Guid{};
            info.sourceId = node.GetLink().sourceId.IsValid() ? node.GetLink().sourceId : node.GetId();
            info.isRoot = isRoot;
            node.SetLink(info);
            for (LevelObjectInstance* child : node.GetChildren())
            {
                link(*child, false);
            }
        };
        link(object, true);
        LevelObjectInstance* refreshed = LevelSerializer::RefreshLinkedInstance(object);
        if (refreshed)
        {
            RecordObjectChange(id, before, "Create LevelObject");
            Select(refreshed->GetId());
        }
        Core::Log::Info("Created LevelObject '" + path + "'");
        return asset;
    }

    Core::Guid EditorContext::CreateTemplateAsset(LevelObjectInstance* object, const std::string& folder)
    {
        JsonDocument document;
        if (object)
        {
            LevelSerializer::MakeLevelObjectDocument(*object, document);
        }
        else
        {
            Level::Level scratch;
            LevelObjectInstance* empty = scratch.CreateObject(LevelObjectInstance::StaticType());
            empty->SetName("New Template");
            LevelSerializer::MakeLevelObjectDocument(*empty, document);
        }
        document["$asset"].SetString("LevelObjectTemplate");
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        std::string name = object ? object->GetName() : "New Template";
        std::string path = database.MakeUniquePath((folder.empty() ? "" : folder + "/") + name + ".template");
        return database.CreateJsonAsset(path, document);
    }

    Core::Guid EditorContext::CreateLevelObjectFromTemplate(const Core::Guid& templateAsset)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        auto asset = database.Load<Level::LevelObjectTemplateAsset>(templateAsset);
        const Assets::AssetRecord* record = database.FindRecord(templateAsset);
        if (!asset || !record || !asset->GetRoot())
        {
            return {};
        }
        Level::Level scratch;
        LevelObjectInstance* copy = LevelSerializer::InstantiateNode(scratch, *asset->GetRoot(), nullptr, nullptr, false);
        if (!copy)
        {
            return {};
        }
        JsonDocument document;
        LevelSerializer::MakeLevelObjectDocument(*copy, document);
        std::filesystem::path templatePath(record->path);
        std::string folder = templatePath.parent_path().generic_string();
        std::string name = templatePath.filename().string();
        name = name.substr(0, name.find('.'));
        std::string path = database.MakeUniquePath((folder.empty() ? "" : folder + "/") + name + ".levelobject");
        Core::Guid created = database.CreateJsonAsset(path, document);
        if (created.IsValid())
        {
            Core::Log::Info("Created LevelObject '" + path + "' from template");
        }
        return created;
    }
}
