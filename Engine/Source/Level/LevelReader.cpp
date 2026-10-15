#include "Level/LevelReader.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Level/Level.h"
#include "Level/LevelAssets.h"
#include "Level/LevelFileSchema.h"
#include "Level/LevelOverrides.h"
#include "Level/LevelSerializer.h"
#include "Level/MissingComponent.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"

namespace BrightBox3D::Level
{
    using Serialization::FindMember;
    using Serialization::GetBool;
    using Serialization::GetString;
    using Serialization::JsonValue;

    namespace
    {
        constexpr const char* EmptyProperties = "{}";

        std::unique_ptr<LevelObjectComponent> CreateComponent(const std::string& typeName, const JsonValue* properties)
        {
            const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(typeName);
            if (type && type->IsA(LevelObjectComponent::StaticType()) && !type->IsA(Transform::StaticType()) && !type->IsAbstract())
            {
                std::unique_ptr<LevelObjectComponent> component(static_cast<LevelObjectComponent*>(type->CreateInstance()));
                if (properties)
                {
                    Serialization::DeserializeProperties(*component, *properties);
                }
                return component;
            }
            if (type && !type->IsA(LevelObjectComponent::StaticType()))
            {
                Core::Log::Info(Core::Text(Core::EngineText::ComponentDropped, typeName, type->GetName()));
                return nullptr;
            }
            auto missing = std::make_unique<MissingComponent>();
            missing->SetMissing(typeName, properties ? Serialization::WriteJson(*properties, false) : EmptyProperties);
            Core::Log::Warning(Core::Text(Core::EngineText::MissingComponentType, typeName));
            return missing;
        }

        std::unique_ptr<LevelObjectComponent> ReadComponent(const JsonValue& entry)
        {
            std::unique_ptr<LevelObjectComponent> component = CreateComponent(GetString(entry, Serialization::TypeKey), FindMember(entry, LevelFileSchema::Properties));
            if (!component)
            {
                return nullptr;
            }
            Core::Guid localId = Core::Guid::Parse(GetString(entry, LevelFileSchema::Id));
            component->SetLocalId(localId.IsValid() ? localId : Core::Guid::Generate());
            component->SetEnabled(GetBool(entry, LevelFileSchema::Enabled, true));
            return component;
        }

        void ReadTransform(LevelObject& object, const JsonValue& node)
        {
            if (const JsonValue* transform = FindMember(node, LevelFileSchema::Transform))
            {
                Serialization::DeserializeProperties(object.GetTransform(), *transform);
            }
        }

        LevelObject* AdoptMissingLinkedNode(Level& level, LevelCollection* collection, LevelObject* parent, const JsonValue& node, const Core::Guid& asset)
        {
            Core::Log::Warning(Core::Text(Core::EngineText::LinkedAssetMissing, LevelObject::StaticType().GetName(), asset.ToString()));
            std::unique_ptr<LevelObject> placeholder = std::make_unique<LevelObject>();
            placeholder->SetName(GetString(node, LevelFileSchema::Name, Core::Text(Core::EngineText::MissingNamed, LevelObject::StaticType().GetName())));
            placeholder->SetMissingType(LevelFileSchema::MissingLevelObjectType, Serialization::WriteJson(node, false));
            ReadTransform(*placeholder, node);
            return level.AdoptObject(std::move(placeholder), collection, parent, false);
        }

        void ReadForeignChildren(Level& level, LevelCollection* collection, LevelObject& root, const JsonValue& node, const LevelObjectLinking::SourceIndex& bySource, bool freshIds)
        {
            const JsonValue* children = FindMember(node, LevelFileSchema::Children);
            if (!children || !children->IsArray())
            {
                return;
            }
            for (const JsonValue& child : children->GetArray())
            {
                auto it = bySource.find(Core::Guid::Parse(GetString(child, LevelFileSchema::AttachTo)));
                LevelObject* attach = it == bySource.end() ? &root : it->second;
                LevelReader::ReadContext childContext;
                childContext.freshIds = freshIds;
                LevelReader::ReadNode(level, collection, attach, child, childContext, false);
            }
        }

        LevelObject* ReadLinkedNode(Level& level, LevelCollection* collection, LevelObject* parent, const JsonValue& node, const Core::Guid& id, bool freshIds)
        {
            Core::Guid asset = Core::Guid::Parse(GetString(node, LevelFileSchema::LevelObjectAsset));
            LevelObjectLibrary::Get().GetPrototype(asset);
            LevelObject* root = LevelReader::InstantiateAsset(level, asset, collection, parent, id, false, true);
            if (!root)
            {
                return AdoptMissingLinkedNode(level, collection, parent, node, asset);
            }
            root->SetName(GetString(node, LevelFileSchema::Name, root->GetName()));
            root->SetActive(GetBool(node, LevelFileSchema::Active, root->IsActiveSelf()));
            ReadTransform(*root, node);
            LevelObjectLinking::SourceIndex bySource = LevelObjectLinking::IndexLinkedBySource(*root);
            if (const JsonValue* overrides = FindMember(node, LevelFileSchema::Overrides); overrides && overrides->IsArray())
            {
                LevelOverrides::ApplyOverrides(*overrides, bySource);
            }
            ReadForeignChildren(level, collection, *root, node, bySource, freshIds);
            return root;
        }

        LevelObject* ReadVariantOfTemplate(Level& level, LevelCollection* collection, LevelObject* parent, const JsonValue& node, const LevelReader::ReadContext& context)
        {
            Core::Guid parentAsset = Core::Guid::Parse(GetString(node, LevelFileSchema::LevelObjectAsset));
            const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(parentAsset);
            if (!record || record->type != Assets::AssetType::LevelObjectTemplate)
            {
                Core::Log::Error(Core::Text(Core::EngineText::VariantParentInvalid, Assets::AssetTypeName(Assets::AssetType::LevelObjectVariant), Assets::AssetTypeName(Assets::AssetType::LevelObjectTemplate)));
                return nullptr;
            }
            LevelObject* root = ReadLinkedNode(level, collection, parent, node, context.derivedRoot, false);
            if (root)
            {
                root->SetLink({ context.asset, root->GetLink().sourceId, true });
            }
            return root;
        }

        std::unique_ptr<LevelObject> CreateObject(const std::string& typeName, bool& missing)
        {
            const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(typeName);
            missing = !type || !type->IsA(LevelObject::StaticType()) || type->IsAbstract();
            std::unique_ptr<Reflection::EngineObject> created((missing ? &LevelObject::StaticType() : type)->CreateInstance());
            return std::unique_ptr<LevelObject>(static_cast<LevelObject*>(created.release()));
        }

        Core::Guid AssignIdentity(LevelObject& object, const Core::Guid& sourceId, const LevelReader::ReadContext& context, bool isAssetRoot)
        {
            if (context.asset.IsValid())
            {
                object.SetLink({ isAssetRoot ? context.asset : Core::Guid{}, sourceId, isAssetRoot });
                return isAssetRoot ? context.derivedRoot : LevelSerializer::DeriveId(context.derivedRoot, sourceId);
            }
            return context.freshIds || !sourceId.IsValid() ? Core::Guid::Generate() : sourceId;
        }

        void ReadObjectFields(LevelObject& object, const JsonValue& node, const std::string& typeName, bool missing)
        {
            object.SetName(GetString(node, LevelFileSchema::Name, LevelObject::StaticType().GetName()));
            object.SetActive(GetBool(node, LevelFileSchema::Active, true));
            const JsonValue* properties = FindMember(node, LevelFileSchema::Properties);
            if (missing)
            {
                object.SetMissingType(typeName, properties ? Serialization::WriteJson(*properties, false) : EmptyProperties);
                Core::Log::Warning(Core::Text(Core::EngineText::MissingObjectType, LevelObject::StaticType().GetName(), typeName, object.GetName()));
            }
            else if (properties)
            {
                Serialization::DeserializeProperties(object, *properties);
            }
            ReadTransform(object, node);
        }

        void ReadComponents(LevelObject& object, const JsonValue& node)
        {
            const JsonValue* components = FindMember(node, LevelFileSchema::Components);
            if (!components || !components->IsArray())
            {
                return;
            }
            for (const JsonValue& entry : components->GetArray())
            {
                if (std::unique_ptr<LevelObjectComponent> component = ReadComponent(entry))
                {
                    Level::AttachComponent(object, std::move(component));
                }
            }
        }

        LevelObject* InitializeIfRequested(Level& level, LevelObject* object, bool initialize)
        {
            if (object && initialize)
            {
                level.InitializeHierarchy(*object);
            }
            return object;
        }
    }

    LevelObject* LevelReader::ReadNode(Level& level, LevelCollection* collection, LevelObject* parent, const JsonValue& node, ReadContext& context, bool isAssetRoot)
    {
        if (!node.IsObject())
        {
            return nullptr;
        }
        if (FindMember(node, LevelFileSchema::LevelObjectAsset))
        {
            if (!context.asset.IsValid())
            {
                Core::Guid id = context.freshIds ? Core::Guid::Generate() : Core::Guid::Parse(GetString(node, LevelFileSchema::Id));
                return ReadLinkedNode(level, collection, parent, node, id, context.freshIds);
            }
            if (isAssetRoot)
            {
                return ReadVariantOfTemplate(level, collection, parent, node, context);
            }
        }

        std::string typeName = GetString(node, Serialization::TypeKey, LevelObject::StaticType().GetName());
        bool missing = false;
        std::unique_ptr<LevelObject> object = CreateObject(typeName, missing);
        Core::Guid sourceId = Core::Guid::Parse(GetString(node, LevelFileSchema::Id));
        Core::Guid id = AssignIdentity(*object, sourceId, context, isAssetRoot);
        ReadObjectFields(*object, node, typeName, missing);
        ReadComponents(*object, node);

        LevelObjectRuntimeState state = Level::GetRuntimeState(*object);
        state.object.id = id;
        Level::SetRuntimeState(*object, state);
        LevelObject* raw = level.AdoptObject(std::move(object), collection, parent, false);
        if (context.bySource && sourceId.IsValid())
        {
            (*context.bySource)[sourceId] = raw;
        }
        if (const JsonValue* children = FindMember(node, LevelFileSchema::Children); children && children->IsArray())
        {
            for (const JsonValue& child : children->GetArray())
            {
                ReadNode(level, collection, raw, child, context, false);
            }
        }
        return raw;
    }

    const JsonValue* LevelReader::FindAssetRoot(const Core::Guid& asset, std::shared_ptr<Assets::Asset>& holder, bool allowTemplate)
    {
        holder = Assets::AssetDatabase::Get().LoadAsset(asset);
        if (auto* variant = holder ? Reflection::Cast<LevelObjectVariantAsset>(holder.get()) : nullptr)
        {
            return variant->GetRoot();
        }
        auto* templateAsset = holder ? Reflection::Cast<LevelObjectTemplateAsset>(holder.get()) : nullptr;
        if (templateAsset && allowTemplate)
        {
            return templateAsset->GetRoot();
        }
        if (templateAsset)
        {
            Core::Log::Error(Core::Text(Core::EngineText::TemplateNotPlaceable, Assets::AssetTypeName(Assets::AssetType::LevelObjectTemplate)));
        }
        return nullptr;
    }

    LevelObject* LevelReader::InstantiateAsset(Level& level, const Core::Guid& asset, LevelCollection* collection, LevelObject* parent, const Core::Guid& instanceId, bool initialize, bool allowTemplate)
    {
        std::shared_ptr<Assets::Asset> holder;
        const JsonValue* root = FindAssetRoot(asset, holder, allowTemplate);
        if (!root)
        {
            return nullptr;
        }
        ReadContext context;
        context.asset = asset;
        context.derivedRoot = instanceId.IsValid() ? instanceId : Core::Guid::Generate();
        return InitializeIfRequested(level, ReadNode(level, collection, parent, *root, context, true), initialize);
    }

    void LevelReader::TagForEditing(LevelObject& node)
    {
        LevelObjectLink link;
        link.sourceId = node.GetId();
        node.SetLink(link);
        for (LevelObject* child : node.GetChildren())
        {
            TagForEditing(*child);
        }
    }

    LevelObject* LevelSerializer::ReadObject(Level& level, LevelCollection* collection, LevelObject* parent, const JsonValue& node, bool initialize)
    {
        LevelReader::ReadContext context;
        return InitializeIfRequested(level, LevelReader::ReadNode(level, collection, parent, node, context, false), initialize);
    }

    LevelObject* LevelSerializer::InstantiateNode(Level& level, const JsonValue& root, LevelCollection* collection, LevelObject* parent, bool initialize)
    {
        LevelReader::ReadContext context;
        context.freshIds = true;
        return InitializeIfRequested(level, LevelReader::ReadNode(level, collection, parent, root, context, false), initialize);
    }

    LevelObject* LevelSerializer::Instantiate(Level& level, const Core::Guid& variant, LevelCollection* collection, LevelObject* parent, const Core::Guid& instanceId, bool initialize)
    {
        LevelObjectLibrary::Get().GetPrototype(variant);
        return LevelReader::InstantiateAsset(level, variant, collection, parent, instanceId, initialize, false);
    }

    bool LevelSerializer::ReadCollection(Level& level, LevelCollection& collection, const JsonValue& document)
    {
        if (!document.IsObject())
        {
            return false;
        }
        std::string name = GetString(document, LevelFileSchema::Name);
        if (!name.empty())
        {
            collection.SetName(name);
        }
        std::vector<LevelObject*> roots;
        if (const JsonValue* objects = FindMember(document, LevelFileSchema::Objects); objects && objects->IsArray())
        {
            for (const JsonValue& node : objects->GetArray())
            {
                if (LevelObject* object = ReadObject(level, &collection, nullptr, node, false))
                {
                    roots.push_back(object);
                }
            }
        }
        for (LevelObject* root : roots)
        {
            level.InitializeHierarchy(*root);
        }
        return true;
    }

    bool LevelSerializer::LoadCollectionInto(Level& level, const Core::Guid& collectionAsset, LevelCollection** loaded)
    {
        auto asset = Assets::AssetDatabase::Get().Load<LevelCollectionAsset>(collectionAsset);
        if (!asset)
        {
            Core::Log::Error(Core::Text(Core::EngineText::CollectionLoadFailed, Assets::AssetTypeName(Assets::AssetType::LevelCollection), collectionAsset.ToString()));
            return false;
        }
        LevelCollection* collection = level.AddCollection(GetString(asset->GetDocument(), LevelFileSchema::Name, asset->GetName()), collectionAsset);
        bool result = ReadCollection(level, *collection, asset->GetDocument());
        collection->SetDirty(false);
        if (loaded)
        {
            *loaded = collection;
        }
        return result;
    }

    std::unique_ptr<Level> LevelSerializer::LoadLevel(const Core::Guid& levelAsset, std::string* error)
    {
        auto asset = Assets::AssetDatabase::Get().Load<LevelAsset>(levelAsset);
        if (!asset)
        {
            if (error)
            {
                *error = Core::Text(Core::EngineText::LevelAssetNotFound);
            }
            return nullptr;
        }
        auto level = std::make_unique<Level>(asset->GetName());
        level->SetAsset(levelAsset);
        const JsonValue& document = asset->GetDocument();
        if (const JsonValue* settings = FindMember(document, LevelFileSchema::Settings); settings && settings->IsObject())
        {
            level->GetSettings().CopyFrom(*settings, level->GetSettings().GetAllocator());
        }
        if (const JsonValue* collections = FindMember(document, LevelFileSchema::Collections); collections && collections->IsArray())
        {
            for (const JsonValue& entry : collections->GetArray())
            {
                Core::Guid guid = Core::Guid::Parse(GetString(entry, LevelFileSchema::Collection));
                if (!LoadCollectionInto(*level, guid))
                {
                    LevelCollection* missing = level->AddCollection(GetString(entry, LevelFileSchema::Name, Core::Text(Core::EngineText::MissingNamed, Assets::AssetTypeName(Assets::AssetType::LevelCollection))), guid);
                    missing->SetDirty(false);
                }
            }
        }
        return level;
    }
}
