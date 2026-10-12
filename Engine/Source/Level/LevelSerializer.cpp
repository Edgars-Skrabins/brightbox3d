#include "Level/LevelSerializer.h"

#include "Assets/AssetDatabase.h"
#include "Core/Hash.h"
#include "Core/Log.h"
#include "Level/LevelAssets.h"
#include "Level/MissingComponent.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"

#include <filesystem>
#include <functional>

namespace BrightBox3D::Level
{
    using Serialization::FindMember;
    using Serialization::GetBool;
    using Serialization::GetString;
    using Serialization::JsonAllocator;
    using Serialization::JsonDocument;
    using Serialization::JsonValue;
    using Serialization::MakeString;

    class LevelSerializerAccess
    {
    public:
        static void Attach(LevelObjectInstance& object, std::unique_ptr<Component> component)
        {
            component->m_owner = &object;
            object.m_components.push_back(std::move(component));
        }

        static void SetId(LevelObjectInstance& object, const Core::Guid& id)
        {
            object.SetId(id);
        }
    };

    namespace
    {
        constexpr const char* TransformTarget = "transform";
        constexpr const char* MissingLevelObjectType = "$MissingLevelObject";

        void WriteRawJson(const std::string& text, JsonValue& out, JsonAllocator& allocator)
        {
            JsonDocument document;
            if (!text.empty() && Serialization::ParseJson(text, document))
            {
                out.CopyFrom(document, allocator);
            }
            else
            {
                out.SetObject();
            }
        }

        void WriteComponent(const Component& component, JsonValue& out, JsonAllocator& allocator)
        {
            out.SetObject();
            out.AddMember("id", MakeString(component.GetLocalId().ToString(), allocator), allocator);
            JsonValue properties(rapidjson::kObjectType);
            if (const auto* missing = Reflection::Cast<MissingComponent>(&component))
            {
                out.AddMember("$type", MakeString(missing->GetMissingTypeName(), allocator), allocator);
                WriteRawJson(missing->GetData(), properties, allocator);
            }
            else
            {
                out.AddMember("$type", MakeString(component.GetType().GetName(), allocator), allocator);
                Serialization::SerializeProperties(component, properties, allocator);
            }
            out.AddMember("enabled", JsonValue(component.IsEnabled()), allocator);
            out.AddMember("properties", properties, allocator);
        }

        bool IsInSameLink(const LevelObjectInstance& object, const LevelObjectInstance& linkRoot)
        {
            if (&object == &linkRoot)
            {
                return true;
            }
            if (!object.GetLink().sourceId.IsValid() || object.GetLink().isRoot)
            {
                return false;
            }
            return const_cast<LevelObjectInstance&>(object).GetLinkRoot() == &linkRoot;
        }

        void CollectLinkedSubtree(LevelObjectInstance& linkRoot, LevelObjectInstance& object, std::vector<LevelObjectInstance*>& linked, std::vector<std::pair<LevelObjectInstance*, LevelObjectInstance*>>& foreign)
        {
            linked.push_back(&object);
            for (LevelObjectInstance* child : object.GetChildren())
            {
                if (IsInSameLink(*child, linkRoot))
                {
                    CollectLinkedSubtree(linkRoot, *child, linked, foreign);
                }
                else
                {
                    foreign.emplace_back(&object, child);
                }
            }
        }

        std::string ComponentKey(const LevelObjectInstance& object, const Component* component)
        {
            if (!component)
            {
                return {};
            }
            if (component == &object.GetTransform())
            {
                return TransformTarget;
            }
            return component->GetLocalId().ToString();
        }

        Component* ResolveComponent(LevelObjectInstance& object, const std::string& key)
        {
            if (key.empty())
            {
                return nullptr;
            }
            if (key == TransformTarget)
            {
                return &object.GetTransform();
            }
            return object.FindComponentByLocalId(Core::Guid::Parse(key));
        }

        bool PseudoPropertiesEqual(const LevelObjectInstance& a, const Component* componentA, const LevelObjectInstance& b, const Component* componentB, const std::string& property)
        {
            if (property == "$name")
            {
                return a.GetName() == b.GetName();
            }
            if (property == "$active")
            {
                return a.IsActiveSelf() == b.IsActiveSelf();
            }
            if (property == "$enabled")
            {
                return componentA && componentB && componentA->IsEnabled() == componentB->IsEnabled();
            }
            return true;
        }

        void WritePseudoProperty(const LevelObjectInstance& object, const Component* component, const std::string& property, JsonValue& out, JsonAllocator& allocator)
        {
            if (property == "$name")
            {
                out = MakeString(object.GetName(), allocator);
            }
            else if (property == "$active")
            {
                out.SetBool(object.IsActiveSelf());
            }
            else if (property == "$enabled")
            {
                out.SetBool(component && component->IsEnabled());
            }
        }

        void ReadPseudoProperty(LevelObjectInstance& object, Component* component, const std::string& property, const JsonValue& value)
        {
            if (property == "$name" && value.IsString())
            {
                object.SetName(value.GetString());
            }
            else if (property == "$active" && value.IsBool())
            {
                object.SetActive(value.GetBool());
            }
            else if (property == "$enabled" && value.IsBool() && component)
            {
                component->SetEnabled(value.GetBool());
            }
        }

        void ForEachOverrideCandidate(const LevelObjectInstance& live, const LevelObjectInstance& prototype, bool isRoot, const std::function<void(const Component*, const Component*, const std::string&, bool)>& visitor)
        {
            if (!isRoot)
            {
                visitor(nullptr, nullptr, "$name", PseudoPropertiesEqual(live, nullptr, prototype, nullptr, "$name"));
                visitor(nullptr, nullptr, "$active", PseudoPropertiesEqual(live, nullptr, prototype, nullptr, "$active"));
                for (const Reflection::PropertyInfo* property : live.GetTransform().GetType().GetAllProperties())
                {
                    visitor(&live.GetTransform(), &prototype.GetTransform(), property->name, Serialization::PropertyValuesEqual(*property, live.GetTransform(), prototype.GetTransform()));
                }
            }
            if (&live.GetType() == &prototype.GetType() && live.GetMissingTypeName().empty())
            {
                for (const Reflection::PropertyInfo* property : live.GetType().GetAllProperties())
                {
                    if (property->IsSerialized())
                    {
                        visitor(nullptr, nullptr, property->name, Serialization::PropertyValuesEqual(*property, live, prototype));
                    }
                }
            }
            for (const auto& component : live.GetComponents())
            {
                const Component* other = prototype.FindComponentByLocalId(component->GetLocalId());
                if (!other || &other->GetType() != &component->GetType() || Reflection::Cast<MissingComponent>(component.get()))
                {
                    continue;
                }
                visitor(component.get(), other, "$enabled", PseudoPropertiesEqual(live, component.get(), prototype, other, "$enabled"));
                for (const Reflection::PropertyInfo* property : component->GetType().GetAllProperties())
                {
                    if (property->IsSerialized())
                    {
                        visitor(component.get(), other, property->name, Serialization::PropertyValuesEqual(*property, *component, *other));
                    }
                }
            }
        }

        void WriteOverrides(const LevelObjectInstance& linkRoot, JsonValue& out, JsonAllocator& allocator)
        {
            out.SetArray();
            LevelObjectLibrary& library = LevelObjectLibrary::Get();
            std::vector<LevelObjectInstance*> linked;
            std::vector<std::pair<LevelObjectInstance*, LevelObjectInstance*>> foreign;
            CollectLinkedSubtree(const_cast<LevelObjectInstance&>(linkRoot), const_cast<LevelObjectInstance&>(linkRoot), linked, foreign);
            for (LevelObjectInstance* object : linked)
            {
                LevelObjectInstance* prototype = library.FindPrototypeNode(linkRoot.GetLink().asset, object->GetLink().sourceId);
                if (!prototype)
                {
                    continue;
                }
                ForEachOverrideCandidate(*object, *prototype, object == &linkRoot, [&](const Component* component, const Component*, const std::string& property, bool equal)
                {
                    if (equal)
                    {
                        return;
                    }
                    JsonValue entry(rapidjson::kObjectType);
                    entry.AddMember("target", MakeString(object->GetLink().sourceId.ToString(), allocator), allocator);
                    std::string key = ComponentKey(*object, component);
                    if (!key.empty())
                    {
                        entry.AddMember("component", MakeString(key, allocator), allocator);
                    }
                    entry.AddMember("property", MakeString(property, allocator), allocator);
                    JsonValue value;
                    if (property[0] == '$')
                    {
                        WritePseudoProperty(*object, component, property, value, allocator);
                    }
                    else
                    {
                        const Reflection::Object& target = component ? static_cast<const Reflection::Object&>(*component) : static_cast<const Reflection::Object&>(*object);
                        Serialization::WriteProperty(*target.GetType().FindProperty(property), target, value, allocator);
                    }
                    entry.AddMember("value", value, allocator);
                    out.PushBack(entry, allocator);
                });
            }
        }

        void WriteNode(const LevelObjectInstance& object, JsonValue& out, JsonAllocator& allocator, bool preserveLinks, bool assetMode);

        void WriteLinkedNode(const LevelObjectInstance& root, JsonValue& out, JsonAllocator& allocator)
        {
            out.SetObject();
            out.AddMember("id", MakeString(root.GetId().ToString(), allocator), allocator);
            out.AddMember("levelObject", MakeString(root.GetLink().asset.ToString(), allocator), allocator);
            out.AddMember("name", MakeString(root.GetName(), allocator), allocator);
            out.AddMember("active", JsonValue(root.IsActiveSelf()), allocator);
            JsonValue transform(rapidjson::kObjectType);
            Serialization::SerializeProperties(root.GetTransform(), transform, allocator);
            out.AddMember("transform", transform, allocator);
            JsonValue overrides;
            WriteOverrides(root, overrides, allocator);
            out.AddMember("overrides", overrides, allocator);

            std::vector<LevelObjectInstance*> linked;
            std::vector<std::pair<LevelObjectInstance*, LevelObjectInstance*>> foreign;
            CollectLinkedSubtree(const_cast<LevelObjectInstance&>(root), const_cast<LevelObjectInstance&>(root), linked, foreign);
            JsonValue children(rapidjson::kArrayType);
            for (auto& [attachTo, child] : foreign)
            {
                if (child->GetHideInLevelContents())
                {
                    continue;
                }
                JsonValue node;
                WriteNode(*child, node, allocator, true, false);
                node.AddMember("attachTo", MakeString(attachTo->GetLink().sourceId.ToString(), allocator), allocator);
                children.PushBack(node, allocator);
            }
            out.AddMember("children", children, allocator);
        }

        void WriteNode(const LevelObjectInstance& object, JsonValue& out, JsonAllocator& allocator, bool preserveLinks, bool assetMode)
        {
            if (object.GetMissingTypeName() == MissingLevelObjectType)
            {
                WriteRawJson(object.GetMissingTypeData(), out, allocator);
                return;
            }
            if (preserveLinks && !assetMode && object.GetLink().isRoot && object.GetLink().asset.IsValid())
            {
                WriteLinkedNode(object, out, allocator);
                return;
            }

            out.SetObject();
            Core::Guid id = assetMode && object.GetLink().sourceId.IsValid() ? object.GetLink().sourceId : object.GetId();
            out.AddMember("id", MakeString(id.ToString(), allocator), allocator);
            out.AddMember("name", MakeString(object.GetName(), allocator), allocator);
            JsonValue properties(rapidjson::kObjectType);
            if (!object.GetMissingTypeName().empty())
            {
                out.AddMember("$type", MakeString(object.GetMissingTypeName(), allocator), allocator);
                WriteRawJson(object.GetMissingTypeData(), properties, allocator);
            }
            else
            {
                out.AddMember("$type", MakeString(object.GetType().GetName(), allocator), allocator);
                Serialization::SerializeProperties(object, properties, allocator);
            }
            out.AddMember("active", JsonValue(object.IsActiveSelf()), allocator);
            out.AddMember("properties", properties, allocator);

            JsonValue transform(rapidjson::kObjectType);
            Serialization::SerializeProperties(object.GetTransform(), transform, allocator);
            out.AddMember("transform", transform, allocator);

            JsonValue components(rapidjson::kArrayType);
            for (const auto& component : object.GetComponents())
            {
                if (component->IsPendingDestroy())
                {
                    continue;
                }
                JsonValue entry;
                WriteComponent(*component, entry, allocator);
                components.PushBack(entry, allocator);
            }
            out.AddMember("components", components, allocator);

            JsonValue children(rapidjson::kArrayType);
            for (const LevelObjectInstance* child : object.GetChildren())
            {
                if (child->GetHideInLevelContents() || child->IsPendingDestroy())
                {
                    continue;
                }
                JsonValue node;
                WriteNode(*child, node, allocator, preserveLinks && !assetMode, assetMode);
                children.PushBack(node, allocator);
            }
            out.AddMember("children", children, allocator);
        }

        struct ReadContext
        {
            Core::Guid derivedRoot;
            Core::Guid asset;
            bool freshIds = false;
            std::unordered_map<Core::Guid, LevelObjectInstance*>* bySource = nullptr;
        };

        std::unique_ptr<Component> ReadComponent(const JsonValue& entry)
        {
            std::string typeName = GetString(entry, "$type");
            const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(typeName);
            const JsonValue* properties = FindMember(entry, "properties");
            std::unique_ptr<Component> component;
            if (type && type->IsA(Component::StaticType()) && !type->IsA(Transform::StaticType()) && !type->IsAbstract())
            {
                component.reset(static_cast<Component*>(type->CreateInstance()));
                if (properties)
                {
                    Serialization::DeserializeProperties(*component, *properties);
                }
            }
            else
            {
                auto missing = std::make_unique<MissingComponent>();
                missing->SetMissing(typeName, properties ? Serialization::WriteJson(*properties, false) : "{}");
                Core::Log::Warning("Missing component type '" + typeName + "'; its data is preserved");
                component = std::move(missing);
            }
            Core::Guid localId = Core::Guid::Parse(GetString(entry, "id"));
            component->SetLocalId(localId.IsValid() ? localId : Core::Guid::Generate());
            component->SetEnabled(GetBool(entry, "enabled", true));
            return component;
        }

        LevelObjectInstance* ReadNodeInternal(Level& level, LevelCollection* collection, LevelObjectInstance* parent, const JsonValue& node, ReadContext& context, bool isAssetRoot);

        LevelObjectInstance* ReadLinkedNode(Level& level, LevelCollection* collection, LevelObjectInstance* parent, const JsonValue& node, bool freshIds)
        {
            Core::Guid id = freshIds ? Core::Guid::Generate() : Core::Guid::Parse(GetString(node, "id"));
            Core::Guid asset = Core::Guid::Parse(GetString(node, "levelObject"));
            LevelObjectInstance* root = LevelSerializer::Instantiate(level, asset, collection, parent, id, false);
            if (!root)
            {
                Core::Log::Warning("LevelObject asset " + asset.ToString() + " is missing; the instance is preserved as a placeholder");
                std::unique_ptr<LevelObjectInstance> placeholder = std::make_unique<LevelObjectInstance>();
                placeholder->SetName(GetString(node, "name", "Missing LevelObject"));
                placeholder->SetMissingType(MissingLevelObjectType, Serialization::WriteJson(node, false));
                if (const JsonValue* transform = FindMember(node, "transform"))
                {
                    Serialization::DeserializeProperties(placeholder->GetTransform(), *transform);
                }
                LevelObjectInstance* raw = level.AdoptObject(std::move(placeholder), collection, parent, false);
                return raw;
            }

            root->SetName(GetString(node, "name", root->GetName()));
            root->SetActive(GetBool(node, "active", true));
            if (const JsonValue* transform = FindMember(node, "transform"))
            {
                Serialization::DeserializeProperties(root->GetTransform(), *transform);
            }

            std::unordered_map<Core::Guid, LevelObjectInstance*> bySource;
            std::function<void(LevelObjectInstance&)> index = [&](LevelObjectInstance& object)
            {
                if (IsInSameLink(object, *root))
                {
                    bySource[object.GetLink().sourceId] = &object;
                    for (LevelObjectInstance* child : object.GetChildren())
                    {
                        index(*child);
                    }
                }
            };
            index(*root);

            if (const JsonValue* overrides = FindMember(node, "overrides"); overrides && overrides->IsArray())
            {
                for (const JsonValue& entry : overrides->GetArray())
                {
                    auto it = bySource.find(Core::Guid::Parse(GetString(entry, "target")));
                    const JsonValue* value = FindMember(entry, "value");
                    if (it == bySource.end() || !value)
                    {
                        continue;
                    }
                    LevelObjectInstance& object = *it->second;
                    std::string componentKey = GetString(entry, "component");
                    Component* component = ResolveComponent(object, componentKey);
                    if (!componentKey.empty() && !component)
                    {
                        continue;
                    }
                    std::string property = GetString(entry, "property");
                    if (!property.empty() && property[0] == '$')
                    {
                        ReadPseudoProperty(object, component, property, *value);
                        continue;
                    }
                    Reflection::Object& target = component ? static_cast<Reflection::Object&>(*component) : static_cast<Reflection::Object&>(object);
                    if (const Reflection::PropertyInfo* info = target.GetType().FindProperty(property))
                    {
                        Serialization::ReadProperty(*info, target, *value);
                        target.OnPropertiesChanged();
                    }
                }
            }

            if (const JsonValue* children = FindMember(node, "children"); children && children->IsArray())
            {
                for (const JsonValue& child : children->GetArray())
                {
                    auto it = bySource.find(Core::Guid::Parse(GetString(child, "attachTo")));
                    LevelObjectInstance* attach = it == bySource.end() ? root : it->second;
                    ReadContext childContext;
                    childContext.freshIds = freshIds;
                    ReadNodeInternal(level, collection, attach, child, childContext, false);
                }
            }
            return root;
        }

        LevelObjectInstance* ReadNodeInternal(Level& level, LevelCollection* collection, LevelObjectInstance* parent, const JsonValue& node, ReadContext& context, bool isAssetRoot)
        {
            if (!node.IsObject())
            {
                return nullptr;
            }
            if (FindMember(node, "levelObject") && !context.asset.IsValid())
            {
                return ReadLinkedNode(level, collection, parent, node, context.freshIds);
            }

            std::string typeName = GetString(node, "$type", "LevelObjectInstance");
            const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(typeName);
            bool missing = !type || !type->IsA(LevelObjectInstance::StaticType()) || type->IsAbstract();
            std::unique_ptr<Reflection::Object> created((missing ? &LevelObjectInstance::StaticType() : type)->CreateInstance());
            std::unique_ptr<LevelObjectInstance> object(static_cast<LevelObjectInstance*>(created.release()));

            Core::Guid sourceId = Core::Guid::Parse(GetString(node, "id"));
            Core::Guid id = sourceId;
            if (context.asset.IsValid())
            {
                id = isAssetRoot ? context.derivedRoot : LevelSerializer::DeriveId(context.derivedRoot, sourceId);
                object->SetLink({ isAssetRoot ? context.asset : Core::Guid{}, sourceId, isAssetRoot });
            }
            else if (context.freshIds || !id.IsValid())
            {
                id = Core::Guid::Generate();
            }

            object->SetName(GetString(node, "name", "LevelObject"));
            object->SetActive(GetBool(node, "active", true));
            const JsonValue* properties = FindMember(node, "properties");
            if (missing)
            {
                object->SetMissingType(typeName, properties ? Serialization::WriteJson(*properties, false) : "{}");
                Core::Log::Warning("Missing LevelObjectInstance type '" + typeName + "' for '" + object->GetName() + "'; its data is preserved");
            }
            else if (properties)
            {
                Serialization::DeserializeProperties(*object, *properties);
            }
            if (const JsonValue* transform = FindMember(node, "transform"))
            {
                Serialization::DeserializeProperties(object->GetTransform(), *transform);
            }
            if (const JsonValue* components = FindMember(node, "components"); components && components->IsArray())
            {
                for (const JsonValue& entry : components->GetArray())
                {
                    std::unique_ptr<Component> component = ReadComponent(entry);
                    LevelSerializerAccess::Attach(*object, std::move(component));
                }
            }

            LevelSerializerAccess::SetId(*object, id);
            LevelObjectInstance* raw = level.AdoptObject(std::move(object), collection, parent, false);
            if (context.bySource && sourceId.IsValid())
            {
                (*context.bySource)[sourceId] = raw;
            }

            if (const JsonValue* children = FindMember(node, "children"); children && children->IsArray())
            {
                for (const JsonValue& child : children->GetArray())
                {
                    ReadNodeInternal(level, collection, raw, child, context, false);
                }
            }
            return raw;
        }

        void InitializeRecursive(Level& level, LevelObjectInstance& object)
        {
            level.InitializeObject(object);
            std::vector<LevelObjectInstance*> children = object.GetChildren();
            for (LevelObjectInstance* child : children)
            {
                InitializeRecursive(level, *child);
            }
        }

        const JsonValue* FindAssetRoot(const Core::Guid& asset, std::shared_ptr<Assets::Asset>& holder)
        {
            holder = Assets::AssetDatabase::Get().LoadAsset(asset);
            if (auto* levelObject = holder ? Reflection::Cast<LevelObjectAsset>(holder.get()) : nullptr)
            {
                return levelObject->GetRoot();
            }
            return nullptr;
        }
    }
}

namespace BrightBox3D::Level
{
    Core::Guid LevelSerializer::DeriveId(const Core::Guid& root, const Core::Guid& source)
    {
        Core::Guid id{ Core::HashCombine(root.high, source.high ^ 0xA5A5A5A5A5A5A5A5ull), Core::HashCombine(root.low, source.low ^ 0x5A5A5A5A5A5A5A5Aull) };
        return id.IsValid() ? id : Core::Guid{ 1, 1 };
    }

    void LevelSerializer::WriteObject(const LevelObjectInstance& object, JsonValue& out, JsonAllocator& allocator, bool preserveLinks)
    {
        WriteNode(object, out, allocator, preserveLinks, false);
    }

    LevelObjectInstance* LevelSerializer::ReadObject(Level& level, LevelCollection* collection, LevelObjectInstance* parent, const JsonValue& node, bool initialize)
    {
        ReadContext context;
        LevelObjectInstance* object = ReadNodeInternal(level, collection, parent, node, context, false);
        if (object && initialize)
        {
            InitializeRecursive(level, *object);
        }
        return object;
    }

    LevelObjectInstance* LevelSerializer::InstantiateNode(Level& level, const JsonValue& root, LevelCollection* collection, LevelObjectInstance* parent, bool initialize)
    {
        ReadContext context;
        context.freshIds = true;
        LevelObjectInstance* object = ReadNodeInternal(level, collection, parent, root, context, false);
        if (object && initialize)
        {
            InitializeRecursive(level, *object);
        }
        return object;
    }

    LevelObjectInstance* LevelSerializer::Instantiate(Level& level, const Core::Guid& levelObjectAsset, LevelCollection* collection, LevelObjectInstance* parent, const Core::Guid& instanceId, bool initialize)
    {
        std::shared_ptr<Assets::Asset> holder;
        const JsonValue* root = FindAssetRoot(levelObjectAsset, holder);
        if (!root)
        {
            if (holder && Reflection::Cast<LevelObjectTemplateAsset>(holder.get()))
            {
                Core::Log::Error("LevelObjectTemplates cannot be instantiated; create a LevelObject from the template first");
            }
            return nullptr;
        }
        ReadContext context;
        context.asset = levelObjectAsset;
        context.derivedRoot = instanceId.IsValid() ? instanceId : Core::Guid::Generate();
        LevelObjectInstance* object = ReadNodeInternal(level, collection, parent, *root, context, true);
        if (object && initialize)
        {
            InitializeRecursive(level, *object);
        }
        return object;
    }

    void LevelSerializer::WriteAssetNode(const LevelObjectInstance& root, JsonValue& out, JsonAllocator& allocator)
    {
        WriteNode(root, out, allocator, false, true);
    }

    void LevelSerializer::MakeLevelObjectDocument(const LevelObjectInstance& root, JsonDocument& document)
    {
        document.SetObject();
        auto& allocator = document.GetAllocator();
        document.AddMember("$asset", "LevelObject", allocator);
        document.AddMember("version", 1, allocator);
        JsonValue node;
        WriteAssetNode(root, node, allocator);
        document.AddMember("root", node, allocator);
    }

    void LevelSerializer::WriteCollection(const LevelCollection& collection, JsonValue& out, JsonAllocator& allocator)
    {
        out.SetObject();
        out.AddMember("$asset", "LevelCollection", allocator);
        out.AddMember("version", 1, allocator);
        out.AddMember("name", MakeString(collection.GetName(), allocator), allocator);
        JsonValue objects(rapidjson::kArrayType);
        for (const LevelObjectInstance* root : collection.GetRoots())
        {
            if (root->GetHideInLevelContents() || root->IsPendingDestroy())
            {
                continue;
            }
            JsonValue node;
            WriteNode(*root, node, allocator, true, false);
            objects.PushBack(node, allocator);
        }
        out.AddMember("objects", objects, allocator);
    }

    bool LevelSerializer::ReadCollection(Level& level, LevelCollection& collection, const JsonValue& document)
    {
        if (!document.IsObject())
        {
            return false;
        }
        std::string name = GetString(document, "name");
        if (!name.empty())
        {
            collection.SetName(name);
        }
        std::vector<LevelObjectInstance*> roots;
        if (const JsonValue* objects = FindMember(document, "objects"); objects && objects->IsArray())
        {
            for (const JsonValue& node : objects->GetArray())
            {
                if (LevelObjectInstance* object = ReadObject(level, &collection, nullptr, node, false))
                {
                    roots.push_back(object);
                }
            }
        }
        for (LevelObjectInstance* root : roots)
        {
            InitializeRecursive(level, *root);
        }
        return true;
    }

    void LevelSerializer::WriteLevel(const Level& level, JsonValue& out, JsonAllocator& allocator)
    {
        out.SetObject();
        out.AddMember("$asset", "Level", allocator);
        out.AddMember("version", 1, allocator);
        JsonValue collections(rapidjson::kArrayType);
        for (const auto& collection : level.GetCollections())
        {
            JsonValue entry(rapidjson::kObjectType);
            entry.AddMember("collection", MakeString(collection->GetAsset().ToString(), allocator), allocator);
            entry.AddMember("name", MakeString(collection->GetName(), allocator), allocator);
            collections.PushBack(entry, allocator);
        }
        out.AddMember("collections", collections, allocator);
        JsonValue settings;
        settings.CopyFrom(level.GetSettings(), allocator);
        out.AddMember("settings", settings, allocator);
    }

    bool LevelSerializer::LoadCollectionInto(Level& level, const Core::Guid& collectionAsset, LevelCollection** loaded)
    {
        auto asset = Assets::AssetDatabase::Get().Load<LevelCollectionAsset>(collectionAsset);
        if (!asset)
        {
            Core::Log::Error("LevelCollection " + collectionAsset.ToString() + " could not be loaded");
            return false;
        }
        LevelCollection* collection = level.AddCollection(GetString(asset->GetDocument(), "name", asset->GetName()), collectionAsset);
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
                *error = "Level asset not found";
            }
            return nullptr;
        }
        auto level = std::make_unique<Level>(asset->GetName());
        level->SetAsset(levelAsset);
        const JsonValue& document = asset->GetDocument();
        if (const JsonValue* settings = FindMember(document, "settings"); settings && settings->IsObject())
        {
            level->GetSettings().CopyFrom(*settings, level->GetSettings().GetAllocator());
        }
        if (const JsonValue* collections = FindMember(document, "collections"); collections && collections->IsArray())
        {
            for (const JsonValue& entry : collections->GetArray())
            {
                Core::Guid guid = Core::Guid::Parse(GetString(entry, "collection"));
                if (!LoadCollectionInto(*level, guid))
                {
                    LevelCollection* missing = level->AddCollection(GetString(entry, "name", "Missing Collection"), guid);
                    missing->SetDirty(false);
                }
            }
        }
        return level;
    }

    bool LevelSerializer::SaveCollection(LevelCollection& collection)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        JsonDocument document;
        WriteCollection(collection, document, document.GetAllocator());
        bool saved = false;
        if (collection.GetAsset().IsValid() && database.FindRecord(collection.GetAsset()))
        {
            saved = database.SaveJsonAsset(collection.GetAsset(), document);
        }
        else
        {
            std::string folder;
            if (const Assets::AssetRecord* levelRecord = database.FindRecord(collection.GetLevel().GetAsset()))
            {
                folder = std::filesystem::path(levelRecord->path).parent_path().generic_string();
            }
            std::string path = database.MakeUniquePath((folder.empty() ? "" : folder + "/") + collection.GetLevel().GetName() + "_" + collection.GetName() + ".collection");
            Core::Guid guid = database.CreateJsonAsset(path, document);
            collection.SetAsset(guid);
            saved = guid.IsValid();
        }
        if (saved)
        {
            collection.SetDirty(false);
        }
        return saved;
    }

    bool LevelSerializer::SaveLevel(Level& level, bool onlyDirty)
    {
        bool success = true;
        for (const auto& collection : level.GetCollections())
        {
            if (!onlyDirty || collection->IsDirty() || !collection->GetAsset().IsValid())
            {
                success = SaveCollection(*collection) && success;
            }
        }
        JsonDocument document;
        WriteLevel(level, document, document.GetAllocator());
        if (level.GetAsset().IsValid())
        {
            success = Assets::AssetDatabase::Get().SaveJsonAsset(level.GetAsset(), document) && success;
        }
        return success;
    }

    Core::Guid LevelSerializer::CreateLevelAsset(const std::string& relativePath, std::string* error)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        std::string path = database.MakeUniquePath(relativePath);
        std::string name = std::filesystem::path(path).stem().string();
        std::string folder = std::filesystem::path(path).parent_path().generic_string();

        JsonDocument collection;
        collection.SetObject();
        collection.AddMember("$asset", "LevelCollection", collection.GetAllocator());
        collection.AddMember("version", 1, collection.GetAllocator());
        collection.AddMember("name", "Main", collection.GetAllocator());
        collection.AddMember("objects", JsonValue(rapidjson::kArrayType), collection.GetAllocator());
        Core::Guid collectionGuid = database.CreateJsonAsset(database.MakeUniquePath((folder.empty() ? "" : folder + "/") + name + "_Main.collection"), collection);

        JsonDocument level;
        level.SetObject();
        auto& allocator = level.GetAllocator();
        level.AddMember("$asset", "Level", allocator);
        level.AddMember("version", 1, allocator);
        JsonValue collections(rapidjson::kArrayType);
        JsonValue entry(rapidjson::kObjectType);
        entry.AddMember("collection", MakeString(collectionGuid.ToString(), allocator), allocator);
        entry.AddMember("name", "Main", allocator);
        collections.PushBack(entry, allocator);
        level.AddMember("collections", collections, allocator);
        level.AddMember("settings", JsonValue(rapidjson::kObjectType), allocator);
        Core::Guid guid = database.CreateJsonAsset(path, level);
        if (!guid.IsValid() && error)
        {
            *error = "could not write level asset";
        }
        return guid;
    }

    std::unique_ptr<Level> LevelSerializer::CloneLevel(const Level& source)
    {
        auto clone = std::make_unique<Level>(source.GetName());
        clone->SetAsset(source.GetAsset());
        clone->SetFixedDeltaTime(source.GetFixedDeltaTime());
        for (const auto& collection : source.GetCollections())
        {
            JsonDocument document;
            WriteCollection(*collection, document, document.GetAllocator());
            LevelCollection* copy = clone->AddCollection(collection->GetName(), collection->GetAsset());
            ReadCollection(*clone, *copy, document);
        }
        return clone;
    }

    std::vector<PropertyOverride> LevelSerializer::ComputeOverrides(const LevelObjectInstance& linkRoot)
    {
        std::vector<PropertyOverride> result;
        JsonDocument document;
        JsonValue overrides;
        WriteOverrides(linkRoot, overrides, document.GetAllocator());
        for (const JsonValue& entry : overrides.GetArray())
        {
            PropertyOverride item;
            item.target = Core::Guid::Parse(GetString(entry, "target"));
            std::string component = GetString(entry, "component");
            item.component = component == TransformTarget ? Core::Guid{ 0, 1 } : Core::Guid::Parse(component);
            item.property = GetString(entry, "property");
            result.push_back(item);
        }
        return result;
    }

    bool LevelSerializer::IsPropertyOverridden(const LevelObjectInstance& object, const Component* component, const std::string& property)
    {
        LevelObjectInstance* linkRoot = const_cast<LevelObjectInstance&>(object).GetLinkRoot();
        if (!linkRoot)
        {
            return false;
        }
        bool isRoot = linkRoot == &object;
        if (isRoot && (component == &object.GetTransform() || property == "$name" || property == "$active"))
        {
            return false;
        }
        LevelObjectInstance* prototype = LevelObjectLibrary::Get().FindPrototypeNode(linkRoot->GetLink().asset, object.GetLink().sourceId);
        if (!prototype)
        {
            return false;
        }
        const Component* other = nullptr;
        if (component)
        {
            other = component == &object.GetTransform() ? &prototype->GetTransform() : prototype->FindComponentByLocalId(component->GetLocalId());
            if (!other || &other->GetType() != &component->GetType())
            {
                return false;
            }
        }
        else if (&prototype->GetType() != &object.GetType())
        {
            return false;
        }
        if (property[0] == '$')
        {
            return !PseudoPropertiesEqual(object, component, *prototype, other, property);
        }
        const Reflection::Object& live = component ? static_cast<const Reflection::Object&>(*component) : static_cast<const Reflection::Object&>(object);
        const Reflection::Object& reference = other ? static_cast<const Reflection::Object&>(*other) : static_cast<const Reflection::Object&>(*prototype);
        const Reflection::PropertyInfo* info = live.GetType().FindProperty(property);
        return info && !Serialization::PropertyValuesEqual(*info, live, reference);
    }

    void LevelSerializer::RevertProperty(LevelObjectInstance& object, Component* component, const std::string& property)
    {
        LevelObjectInstance* linkRoot = object.GetLinkRoot();
        if (!linkRoot)
        {
            return;
        }
        LevelObjectInstance* prototype = LevelObjectLibrary::Get().FindPrototypeNode(linkRoot->GetLink().asset, object.GetLink().sourceId);
        if (!prototype)
        {
            return;
        }
        Component* other = nullptr;
        if (component)
        {
            other = component == &object.GetTransform() ? &prototype->GetTransform() : prototype->FindComponentByLocalId(component->GetLocalId());
            if (!other)
            {
                return;
            }
        }
        if (property[0] == '$')
        {
            JsonDocument document;
            JsonValue value;
            WritePseudoProperty(*prototype, other, property, value, document.GetAllocator());
            ReadPseudoProperty(object, component, property, value);
            return;
        }
        Reflection::Object& live = component ? static_cast<Reflection::Object&>(*component) : static_cast<Reflection::Object&>(object);
        Reflection::Object& reference = other ? static_cast<Reflection::Object&>(*other) : static_cast<Reflection::Object&>(*prototype);
        if (const Reflection::PropertyInfo* info = live.GetType().FindProperty(property))
        {
            Serialization::CopyPropertyValue(*info, reference, live);
        }
    }

    namespace
    {
        struct EditableAssetCopy
        {
            std::unique_ptr<Level> level;
            LevelObjectInstance* root = nullptr;
            std::unordered_map<Core::Guid, LevelObjectInstance*> bySource;
        };

        bool MakeEditableAssetCopy(const Core::Guid& asset, EditableAssetCopy& copy)
        {
            copy.level = std::make_unique<Level>("AssetCopy");
            copy.root = LevelSerializer::Instantiate(*copy.level, asset, nullptr, nullptr, Core::Guid::Generate(), false);
            if (!copy.root)
            {
                return false;
            }
            std::function<void(LevelObjectInstance&)> index = [&](LevelObjectInstance& object)
            {
                copy.bySource[object.GetLink().sourceId] = &object;
                for (LevelObjectInstance* child : object.GetChildren())
                {
                    index(*child);
                }
            };
            index(*copy.root);
            return true;
        }

        bool CopyValueInto(const LevelObjectInstance& liveObject, const Component* liveComponent, LevelObjectInstance& targetObject, const std::string& property)
        {
            Component* targetComponent = nullptr;
            if (liveComponent)
            {
                targetComponent = liveComponent == &liveObject.GetTransform() ? &targetObject.GetTransform() : targetObject.FindComponentByLocalId(liveComponent->GetLocalId());
                if (!targetComponent)
                {
                    return false;
                }
            }
            if (property[0] == '$')
            {
                JsonDocument document;
                JsonValue value;
                WritePseudoProperty(liveObject, liveComponent, property, value, document.GetAllocator());
                ReadPseudoProperty(targetObject, targetComponent, property, value);
                return true;
            }
            const Reflection::Object& live = liveComponent ? static_cast<const Reflection::Object&>(*liveComponent) : static_cast<const Reflection::Object&>(liveObject);
            Reflection::Object& target = targetComponent ? static_cast<Reflection::Object&>(*targetComponent) : static_cast<Reflection::Object&>(targetObject);
            const Reflection::PropertyInfo* info = live.GetType().FindProperty(property);
            if (!info || &live.GetType() != &target.GetType())
            {
                return false;
            }
            Serialization::CopyPropertyValue(*info, live, target);
            return true;
        }

        bool SaveAssetCopy(const Core::Guid& asset, const EditableAssetCopy& copy)
        {
            JsonDocument document;
            LevelSerializer::MakeLevelObjectDocument(*copy.root, document);
            return Assets::AssetDatabase::Get().SaveJsonAsset(asset, document);
        }
    }

    bool LevelSerializer::ApplyPropertyToAsset(LevelObjectInstance& object, Component* component, const std::string& property)
    {
        LevelObjectInstance* linkRoot = object.GetLinkRoot();
        if (!linkRoot)
        {
            return false;
        }
        const Core::Guid asset = linkRoot->GetLink().asset;
        EditableAssetCopy copy;
        if (!MakeEditableAssetCopy(asset, copy))
        {
            return false;
        }
        auto it = copy.bySource.find(object.GetLink().sourceId);
        if (it == copy.bySource.end() || !CopyValueInto(object, component, *it->second, property))
        {
            return false;
        }
        return SaveAssetCopy(asset, copy);
    }

    bool LevelSerializer::ApplyAllToAsset(LevelObjectInstance& linkRoot)
    {
        if (!linkRoot.GetLink().isRoot)
        {
            return false;
        }
        const Core::Guid asset = linkRoot.GetLink().asset;
        EditableAssetCopy copy;
        if (!MakeEditableAssetCopy(asset, copy))
        {
            return false;
        }
        LevelObjectLibrary& library = LevelObjectLibrary::Get();
        std::vector<LevelObjectInstance*> linked;
        std::vector<std::pair<LevelObjectInstance*, LevelObjectInstance*>> foreign;
        CollectLinkedSubtree(linkRoot, linkRoot, linked, foreign);
        for (LevelObjectInstance* object : linked)
        {
            LevelObjectInstance* prototype = library.FindPrototypeNode(asset, object->GetLink().sourceId);
            auto target = copy.bySource.find(object->GetLink().sourceId);
            if (!prototype || target == copy.bySource.end())
            {
                continue;
            }
            ForEachOverrideCandidate(*object, *prototype, object == &linkRoot, [&](const Component* component, const Component*, const std::string& property, bool equal)
            {
                if (!equal)
                {
                    CopyValueInto(*object, component, *target->second, property);
                }
            });
        }
        return SaveAssetCopy(asset, copy);
    }

    void LevelSerializer::RevertAll(LevelObjectInstance& linkRoot)
    {
        if (!linkRoot.GetLink().isRoot)
        {
            return;
        }
        JsonDocument document;
        WriteLinkedNode(linkRoot, document, document.GetAllocator());
        document["overrides"].SetArray();
        Level* level = linkRoot.GetLevel();
        LevelObjectInstance* parent = linkRoot.GetParent();
        LevelCollection* collection = linkRoot.GetCollection();
        int sibling = linkRoot.GetSiblingIndex();
        level->DestroyObjectImmediate(&linkRoot);
        if (LevelObjectInstance* restored = ReadObject(*level, collection, parent, document, true))
        {
            restored->SetSiblingIndex(sibling);
        }
    }

    LevelObjectInstance* LevelSerializer::RefreshLinkedInstance(LevelObjectInstance& linkRoot)
    {
        JsonDocument document;
        WriteLinkedNode(linkRoot, document, document.GetAllocator());
        Level* level = linkRoot.GetLevel();
        LevelObjectInstance* parent = linkRoot.GetParent();
        LevelCollection* collection = linkRoot.GetCollection();
        int sibling = linkRoot.GetSiblingIndex();
        level->DestroyObjectImmediate(&linkRoot);
        LevelObjectInstance* restored = ReadObject(*level, collection, parent, document, true);
        if (restored)
        {
            restored->SetSiblingIndex(sibling);
        }
        return restored;
    }

    void LevelSerializer::RefreshLinkedInstances(Level& level, const Core::Guid& levelObjectAsset)
    {
        struct Pending
        {
            JsonDocument document;
            LevelObjectInstance* parent = nullptr;
            LevelCollection* collection = nullptr;
            int sibling = 0;
            Core::Guid parentId;
        };
        std::vector<LevelObjectInstance*> roots;
        for (const auto& object : level.GetObjects())
        {
            if (object->GetLink().isRoot && object->GetLink().asset == levelObjectAsset && !object->IsPendingDestroy())
            {
                roots.push_back(object.get());
            }
        }
        std::vector<std::unique_ptr<Pending>> pending;
        for (LevelObjectInstance* root : roots)
        {
            auto entry = std::make_unique<Pending>();
            WriteLinkedNode(*root, entry->document, entry->document.GetAllocator());
            entry->collection = root->GetCollection();
            entry->sibling = root->GetSiblingIndex();
            entry->parentId = root->GetParent() ? root->GetParent()->GetId() : Core::Guid{};
            pending.push_back(std::move(entry));
        }
        LevelObjectLibrary::Get().Invalidate(levelObjectAsset);
        for (LevelObjectInstance* root : roots)
        {
            level.DestroyObjectImmediate(root);
        }
        for (auto& entry : pending)
        {
            LevelObjectInstance* parent = entry->parentId.IsValid() ? level.FindObject(entry->parentId) : nullptr;
            if (LevelObjectInstance* restored = ReadObject(level, entry->collection, parent, entry->document, true))
            {
                restored->SetSiblingIndex(entry->sibling);
            }
        }
    }

    void LevelSerializer::Unlink(LevelObjectInstance& linkRoot)
    {
        std::vector<LevelObjectInstance*> linked;
        std::vector<std::pair<LevelObjectInstance*, LevelObjectInstance*>> foreign;
        CollectLinkedSubtree(linkRoot, linkRoot, linked, foreign);
        for (LevelObjectInstance* object : linked)
        {
            object->ClearLink();
        }
        if (linkRoot.GetLevel())
        {
            linkRoot.GetLevel()->MarkHierarchyChanged();
        }
    }

    LevelObjectLibrary& LevelObjectLibrary::Get()
    {
        static LevelObjectLibrary* library = new LevelObjectLibrary();
        return *library;
    }

    LevelObjectLibrary::LevelObjectLibrary() = default;

    LevelObjectInstance* LevelObjectLibrary::GetPrototype(const Core::Guid& asset)
    {
        auto it = m_entries.find(asset);
        if (it != m_entries.end())
        {
            return it->second.root;
        }
        Entry entry;
        entry.level = std::make_unique<Level>("Prototype");
        entry.root = LevelSerializer::Instantiate(*entry.level, asset, nullptr, nullptr, Core::Guid::Generate(), false);
        if (!entry.root)
        {
            return nullptr;
        }
        std::function<void(LevelObjectInstance&)> index = [&](LevelObjectInstance& object)
        {
            entry.bySource[object.GetLink().sourceId] = &object;
            for (LevelObjectInstance* child : object.GetChildren())
            {
                index(*child);
            }
        };
        index(*entry.root);
        LevelObjectInstance* root = entry.root;
        m_entries[asset] = std::move(entry);
        return root;
    }

    LevelObjectInstance* LevelObjectLibrary::FindPrototypeNode(const Core::Guid& asset, const Core::Guid& sourceId)
    {
        if (!GetPrototype(asset))
        {
            return nullptr;
        }
        auto& bySource = m_entries[asset].bySource;
        auto it = bySource.find(sourceId);
        return it == bySource.end() ? nullptr : it->second;
    }

    void LevelObjectLibrary::Invalidate(const Core::Guid& asset)
    {
        m_entries.erase(asset);
    }

    void LevelObjectLibrary::Clear()
    {
        m_entries.clear();
    }
}
