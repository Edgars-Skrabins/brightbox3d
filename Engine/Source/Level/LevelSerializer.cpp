#include "Level/LevelSerializer.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
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

        void WriteComponent(const LevelObjectComponent& component, JsonValue& out, JsonAllocator& allocator)
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

        bool IsInSameLink(const LevelObject& object, const LevelObject& linkRoot)
        {
            if (&object == &linkRoot)
            {
                return true;
            }
            if (!object.GetLink().sourceId.IsValid() || object.GetLink().isRoot)
            {
                return false;
            }
            return const_cast<LevelObject&>(object).GetLinkRoot() == &linkRoot;
        }

        void CollectLinkedSubtree(LevelObject& linkRoot, LevelObject& object, std::vector<LevelObject*>& linked, std::vector<std::pair<LevelObject*, LevelObject*>>& foreign)
        {
            linked.push_back(&object);
            for (LevelObject* child : object.GetChildren())
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

        std::string ComponentKey(const LevelObject& object, const LevelObjectComponent* component)
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

        LevelObjectComponent* ResolveComponent(LevelObject& object, const std::string& key)
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

        bool PseudoPropertiesEqual(const LevelObject& a, const LevelObjectComponent* componentA, const LevelObject& b, const LevelObjectComponent* componentB, const std::string& property)
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

        void WritePseudoProperty(const LevelObject& object, const LevelObjectComponent* component, const std::string& property, JsonValue& out, JsonAllocator& allocator)
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

        void ReadPseudoProperty(LevelObject& object, LevelObjectComponent* component, const std::string& property, const JsonValue& value)
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

        void ForEachOverrideCandidate(const LevelObject& live, const LevelObject& prototype, bool isRoot, const std::function<void(const LevelObjectComponent*, const LevelObjectComponent*, const std::string&, bool)>& visitor)
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
                const LevelObjectComponent* other = prototype.FindComponentByLocalId(component->GetLocalId());
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

        void WriteOverrides(const LevelObject& linkRoot, JsonValue& out, JsonAllocator& allocator)
        {
            out.SetArray();
            LevelObjectLibrary& library = LevelObjectLibrary::Get();
            std::vector<LevelObject*> linked;
            std::vector<std::pair<LevelObject*, LevelObject*>> foreign;
            CollectLinkedSubtree(const_cast<LevelObject&>(linkRoot), const_cast<LevelObject&>(linkRoot), linked, foreign);
            for (LevelObject* object : linked)
            {
                LevelObject* prototype = library.FindPrototypeNode(linkRoot.GetLink().asset, object->GetLink().sourceId);
                if (!prototype)
                {
                    continue;
                }
                ForEachOverrideCandidate(*object, *prototype, object == &linkRoot, [&](const LevelObjectComponent* component, const LevelObjectComponent*, const std::string& property, bool equal)
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
                        const Reflection::EngineObject& target = component ? static_cast<const Reflection::EngineObject&>(*component) : static_cast<const Reflection::EngineObject&>(*object);
                        Serialization::WriteProperty(*target.GetType().FindProperty(property), target, value, allocator);
                    }
                    entry.AddMember("value", value, allocator);
                    out.PushBack(entry, allocator);
                });
            }
        }

        void WriteNode(const LevelObject& object, JsonValue& out, JsonAllocator& allocator, bool preserveLinks, bool assetMode);

        void WriteLinkedNode(const LevelObject& root, JsonValue& out, JsonAllocator& allocator)
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

            std::vector<LevelObject*> linked;
            std::vector<std::pair<LevelObject*, LevelObject*>> foreign;
            CollectLinkedSubtree(const_cast<LevelObject&>(root), const_cast<LevelObject&>(root), linked, foreign);
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

        void WriteNode(const LevelObject& object, JsonValue& out, JsonAllocator& allocator, bool preserveLinks, bool assetMode)
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
            for (const LevelObject* child : object.GetChildren())
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
            std::unordered_map<Core::Guid, LevelObject*>* bySource = nullptr;
        };

        std::unique_ptr<LevelObjectComponent> ReadComponent(const JsonValue& entry)
        {
            std::string typeName = GetString(entry, "$type");
            const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(typeName);
            const JsonValue* properties = FindMember(entry, "properties");
            std::unique_ptr<LevelObjectComponent> component;
            if (type && type->IsA(LevelObjectComponent::StaticType()) && !type->IsA(Transform::StaticType()) && !type->IsAbstract())
            {
                component.reset(static_cast<LevelObjectComponent*>(type->CreateInstance()));
                if (properties)
                {
                    Serialization::DeserializeProperties(*component, *properties);
                }
            }
            else if (type && !type->IsA(LevelObjectComponent::StaticType()))
            {
                Core::Log::Info(Core::Text(Core::EngineText::ComponentDropped, typeName, type->GetName()));
                return nullptr;
            }
            else
            {
                auto missing = std::make_unique<MissingComponent>();
                missing->SetMissing(typeName, properties ? Serialization::WriteJson(*properties, false) : "{}");
                Core::Log::Warning(Core::Text(Core::EngineText::MissingComponentType, typeName));
                component = std::move(missing);
            }
            Core::Guid localId = Core::Guid::Parse(GetString(entry, "id"));
            component->SetLocalId(localId.IsValid() ? localId : Core::Guid::Generate());
            component->SetEnabled(GetBool(entry, "enabled", true));
            return component;
        }

        LevelObject* ReadNodeInternal(Level& level, LevelCollection* collection, LevelObject* parent, const JsonValue& node, ReadContext& context, bool isAssetRoot);

        LevelObject* InstantiateAsset(Level& level, const Core::Guid& asset, LevelCollection* collection, LevelObject* parent, const Core::Guid& instanceId, bool initialize, bool allowTemplate);

        LevelObject* ReadLinkedNode(Level& level, LevelCollection* collection, LevelObject* parent, const JsonValue& node, const Core::Guid& id, bool freshIds)
        {
            Core::Guid asset = Core::Guid::Parse(GetString(node, "levelObject"));
            LevelObjectLibrary::Get().GetPrototype(asset);
            LevelObject* root = InstantiateAsset(level, asset, collection, parent, id, false, true);
            if (!root)
            {
                Core::Log::Warning(Core::Text(Core::EngineText::LinkedAssetMissing, LevelObject::StaticType().GetName(), asset.ToString()));
                std::unique_ptr<LevelObject> placeholder = std::make_unique<LevelObject>();
                placeholder->SetName(GetString(node, "name", "Missing LevelObject"));
                placeholder->SetMissingType(MissingLevelObjectType, Serialization::WriteJson(node, false));
                if (const JsonValue* transform = FindMember(node, "transform"))
                {
                    Serialization::DeserializeProperties(placeholder->GetTransform(), *transform);
                }
                LevelObject* raw = level.AdoptObject(std::move(placeholder), collection, parent, false);
                return raw;
            }

            root->SetName(GetString(node, "name", root->GetName()));
            root->SetActive(GetBool(node, "active", root->IsActiveSelf()));
            if (const JsonValue* transform = FindMember(node, "transform"))
            {
                Serialization::DeserializeProperties(root->GetTransform(), *transform);
            }

            std::unordered_map<Core::Guid, LevelObject*> bySource;
            std::function<void(LevelObject&)> index = [&](LevelObject& object)
            {
                if (IsInSameLink(object, *root))
                {
                    bySource[object.GetLink().sourceId] = &object;
                    for (LevelObject* child : object.GetChildren())
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
                    LevelObject& object = *it->second;
                    std::string componentKey = GetString(entry, "component");
                    LevelObjectComponent* component = ResolveComponent(object, componentKey);
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
                    Reflection::EngineObject& target = component ? static_cast<Reflection::EngineObject&>(*component) : static_cast<Reflection::EngineObject&>(object);
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
                    LevelObject* attach = it == bySource.end() ? root : it->second;
                    ReadContext childContext;
                    childContext.freshIds = freshIds;
                    ReadNodeInternal(level, collection, attach, child, childContext, false);
                }
            }
            return root;
        }

        LevelObject* ReadVariantOfTemplate(Level& level, LevelCollection* collection, LevelObject* parent, const JsonValue& node, const ReadContext& context)
        {
            Core::Guid parentAsset = Core::Guid::Parse(GetString(node, "levelObject"));
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

        LevelObject* ReadNodeInternal(Level& level, LevelCollection* collection, LevelObject* parent, const JsonValue& node, ReadContext& context, bool isAssetRoot)
        {
            if (!node.IsObject())
            {
                return nullptr;
            }
            if (FindMember(node, "levelObject"))
            {
                if (!context.asset.IsValid())
                {
                    Core::Guid id = context.freshIds ? Core::Guid::Generate() : Core::Guid::Parse(GetString(node, "id"));
                    return ReadLinkedNode(level, collection, parent, node, id, context.freshIds);
                }
                if (isAssetRoot)
                {
                    return ReadVariantOfTemplate(level, collection, parent, node, context);
                }
            }

            std::string typeName = GetString(node, "$type", "LevelObject");
            const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(typeName);
            bool missing = !type || !type->IsA(LevelObject::StaticType()) || type->IsAbstract();
            std::unique_ptr<Reflection::EngineObject> created((missing ? &LevelObject::StaticType() : type)->CreateInstance());
            std::unique_ptr<LevelObject> object(static_cast<LevelObject*>(created.release()));

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
                Core::Log::Warning(Core::Text(Core::EngineText::MissingObjectType, LevelObject::StaticType().GetName(), typeName, object->GetName()));
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
                    if (std::unique_ptr<LevelObjectComponent> component = ReadComponent(entry))
                    {
                        Level::AttachComponent(*object, std::move(component));
                    }
                }
            }

            LevelObjectRuntimeState state = Level::GetRuntimeState(*object);
            state.object.id = id;
            Level::SetRuntimeState(*object, state);
            LevelObject* raw = level.AdoptObject(std::move(object), collection, parent, false);
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

        const JsonValue* FindAssetRoot(const Core::Guid& asset, std::shared_ptr<Assets::Asset>& holder, bool allowTemplate)
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

        LevelObject* InstantiateAsset(Level& level, const Core::Guid& asset, LevelCollection* collection, LevelObject* parent, const Core::Guid& instanceId, bool initialize, bool allowTemplate)
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
            LevelObject* object = ReadNodeInternal(level, collection, parent, *root, context, true);
            if (object && initialize)
            {
                level.InitializeHierarchy(*object);
            }
            return object;
        }

        void TagForEditing(LevelObject& node)
        {
            LevelObjectLink link;
            link.sourceId = node.GetId();
            node.SetLink(link);
            for (LevelObject* child : node.GetChildren())
            {
                TagForEditing(*child);
            }
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

    void LevelSerializer::WriteObject(const LevelObject& object, JsonValue& out, JsonAllocator& allocator, bool preserveLinks)
    {
        WriteNode(object, out, allocator, preserveLinks, false);
    }

    LevelObject* LevelSerializer::ReadObject(Level& level, LevelCollection* collection, LevelObject* parent, const JsonValue& node, bool initialize)
    {
        ReadContext context;
        LevelObject* object = ReadNodeInternal(level, collection, parent, node, context, false);
        if (object && initialize)
        {
            level.InitializeHierarchy(*object);
        }
        return object;
    }

    LevelObject* LevelSerializer::InstantiateNode(Level& level, const JsonValue& root, LevelCollection* collection, LevelObject* parent, bool initialize)
    {
        ReadContext context;
        context.freshIds = true;
        LevelObject* object = ReadNodeInternal(level, collection, parent, root, context, false);
        if (object && initialize)
        {
            level.InitializeHierarchy(*object);
        }
        return object;
    }

    LevelObject* LevelSerializer::Instantiate(Level& level, const Core::Guid& variant, LevelCollection* collection, LevelObject* parent, const Core::Guid& instanceId, bool initialize)
    {
        LevelObjectLibrary::Get().GetPrototype(variant);
        return InstantiateAsset(level, variant, collection, parent, instanceId, initialize, false);
    }

    LevelObject* LevelSerializer::OpenAsset(Level& level, LevelCollection* collection, const Core::Guid& asset)
    {
        std::shared_ptr<Assets::Asset> holder;
        const JsonValue* root = FindAssetRoot(asset, holder, true);
        if (!root)
        {
            return nullptr;
        }
        LevelObject* object = ReadObject(level, collection, nullptr, *root, false);
        if (object && !(object->GetLink().isRoot && object->GetLink().asset.IsValid()))
        {
            TagForEditing(*object);
        }
        return object;
    }

    bool LevelSerializer::SaveAsset(const LevelObject& root, const Core::Guid& asset)
    {
        const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(asset);
        if (!record)
        {
            return false;
        }
        JsonDocument document;
        MakeLevelObjectDocument(root, document, record->type);
        return Assets::AssetDatabase::Get().SaveJsonAsset(asset, document);
    }

    void LevelSerializer::WriteAssetNode(const LevelObject& root, JsonValue& out, JsonAllocator& allocator)
    {
        WriteNode(root, out, allocator, false, true);
    }

    void LevelSerializer::MakeLevelObjectDocument(const LevelObject& root, JsonDocument& document, Assets::AssetType type)
    {
        document.SetObject();
        auto& allocator = document.GetAllocator();
        document.AddMember("$asset", JsonValue(Assets::AssetTypeName(type), allocator), allocator);
        document.AddMember("version", 1, allocator);
        JsonValue node;
        if (type == Assets::AssetType::LevelObjectVariant && root.GetLink().isRoot && root.GetLink().asset.IsValid())
        {
            WriteLinkedNode(root, node, allocator);
        }
        else
        {
            WriteAssetNode(root, node, allocator);
        }
        document.AddMember("root", node, allocator);
    }

    void LevelSerializer::WriteCollection(const LevelCollection& collection, JsonValue& out, JsonAllocator& allocator)
    {
        out.SetObject();
        out.AddMember("$asset", "LevelCollection", allocator);
        out.AddMember("version", 1, allocator);
        out.AddMember("name", MakeString(collection.GetName(), allocator), allocator);
        JsonValue objects(rapidjson::kArrayType);
        for (const LevelObject* root : collection.GetRoots())
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
        std::vector<LevelObject*> roots;
        if (const JsonValue* objects = FindMember(document, "objects"); objects && objects->IsArray())
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
            Core::Log::Error(Core::Text(Core::EngineText::CollectionLoadFailed, Assets::AssetTypeName(Assets::AssetType::LevelCollection), collectionAsset.ToString()));
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
                *error = Core::Text(Core::EngineText::LevelAssetNotFound);
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
            *error = Core::Text(Core::EngineText::LevelWriteFailed);
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

    std::vector<PropertyOverride> LevelSerializer::ComputeOverrides(const LevelObject& linkRoot)
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

    bool LevelSerializer::IsPropertyOverridden(const LevelObject& object, const LevelObjectComponent* component, const std::string& property)
    {
        LevelObject* linkRoot = const_cast<LevelObject&>(object).GetLinkRoot();
        if (!linkRoot)
        {
            return false;
        }
        bool isRoot = linkRoot == &object;
        if (isRoot && (component == &object.GetTransform() || property == "$name" || property == "$active"))
        {
            return false;
        }
        LevelObject* prototype = LevelObjectLibrary::Get().FindPrototypeNode(linkRoot->GetLink().asset, object.GetLink().sourceId);
        if (!prototype)
        {
            return false;
        }
        const LevelObjectComponent* other = nullptr;
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
        const Reflection::EngineObject& live = component ? static_cast<const Reflection::EngineObject&>(*component) : static_cast<const Reflection::EngineObject&>(object);
        const Reflection::EngineObject& reference = other ? static_cast<const Reflection::EngineObject&>(*other) : static_cast<const Reflection::EngineObject&>(*prototype);
        const Reflection::PropertyInfo* info = live.GetType().FindProperty(property);
        return info && !Serialization::PropertyValuesEqual(*info, live, reference);
    }

    void LevelSerializer::RevertProperty(LevelObject& object, LevelObjectComponent* component, const std::string& property)
    {
        LevelObject* linkRoot = object.GetLinkRoot();
        if (!linkRoot)
        {
            return;
        }
        LevelObject* prototype = LevelObjectLibrary::Get().FindPrototypeNode(linkRoot->GetLink().asset, object.GetLink().sourceId);
        if (!prototype)
        {
            return;
        }
        LevelObjectComponent* other = nullptr;
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
        Reflection::EngineObject& live = component ? static_cast<Reflection::EngineObject&>(*component) : static_cast<Reflection::EngineObject&>(object);
        Reflection::EngineObject& reference = other ? static_cast<Reflection::EngineObject&>(*other) : static_cast<Reflection::EngineObject&>(*prototype);
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
            LevelObject* root = nullptr;
            std::unordered_map<Core::Guid, LevelObject*> bySource;
        };

        bool MakeEditableAssetCopy(const Core::Guid& asset, EditableAssetCopy& copy)
        {
            copy.level = std::make_unique<Level>("AssetCopy");
            copy.root = LevelSerializer::OpenAsset(*copy.level, nullptr, asset);
            if (!copy.root)
            {
                return false;
            }
            std::function<void(LevelObject&)> index = [&](LevelObject& object)
            {
                copy.bySource[object.GetLink().sourceId] = &object;
                for (LevelObject* child : object.GetChildren())
                {
                    index(*child);
                }
            };
            index(*copy.root);
            return true;
        }

        bool CopyValueInto(const LevelObject& liveObject, const LevelObjectComponent* liveComponent, LevelObject& targetObject, const std::string& property)
        {
            LevelObjectComponent* targetComponent = nullptr;
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
            const Reflection::EngineObject& live = liveComponent ? static_cast<const Reflection::EngineObject&>(*liveComponent) : static_cast<const Reflection::EngineObject&>(liveObject);
            Reflection::EngineObject& target = targetComponent ? static_cast<Reflection::EngineObject&>(*targetComponent) : static_cast<Reflection::EngineObject&>(targetObject);
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
            return LevelSerializer::SaveAsset(*copy.root, asset);
        }
    }

    bool LevelSerializer::ApplyPropertyToAsset(LevelObject& object, LevelObjectComponent* component, const std::string& property)
    {
        LevelObject* linkRoot = object.GetLinkRoot();
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

    bool LevelSerializer::ApplyAllToAsset(LevelObject& linkRoot)
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
        std::vector<LevelObject*> linked;
        std::vector<std::pair<LevelObject*, LevelObject*>> foreign;
        CollectLinkedSubtree(linkRoot, linkRoot, linked, foreign);
        for (LevelObject* object : linked)
        {
            LevelObject* prototype = library.FindPrototypeNode(asset, object->GetLink().sourceId);
            auto target = copy.bySource.find(object->GetLink().sourceId);
            if (!prototype || target == copy.bySource.end())
            {
                continue;
            }
            ForEachOverrideCandidate(*object, *prototype, object == &linkRoot, [&](const LevelObjectComponent* component, const LevelObjectComponent*, const std::string& property, bool equal)
            {
                if (!equal)
                {
                    CopyValueInto(*object, component, *target->second, property);
                }
            });
        }
        return SaveAssetCopy(asset, copy);
    }

    void LevelSerializer::RevertAll(LevelObject& linkRoot)
    {
        if (!linkRoot.GetLink().isRoot)
        {
            return;
        }
        JsonDocument document;
        WriteLinkedNode(linkRoot, document, document.GetAllocator());
        document["overrides"].SetArray();
        Level* level = linkRoot.GetLevel();
        LevelObject* parent = linkRoot.GetParent();
        LevelCollection* collection = linkRoot.GetCollection();
        int sibling = linkRoot.GetSiblingIndex();
        level->DestroyObjectImmediate(&linkRoot);
        if (LevelObject* restored = ReadObject(*level, collection, parent, document, true))
        {
            restored->SetSiblingIndex(sibling);
        }
    }

    LevelObject* LevelSerializer::RefreshLinkedInstance(LevelObject& linkRoot)
    {
        JsonDocument document;
        WriteLinkedNode(linkRoot, document, document.GetAllocator());
        Level* level = linkRoot.GetLevel();
        LevelObject* parent = linkRoot.GetParent();
        LevelCollection* collection = linkRoot.GetCollection();
        int sibling = linkRoot.GetSiblingIndex();
        level->DestroyObjectImmediate(&linkRoot);
        LevelObject* restored = ReadObject(*level, collection, parent, document, true);
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
            LevelObject* parent = nullptr;
            LevelCollection* collection = nullptr;
            int sibling = 0;
            Core::Guid parentId;
        };
        std::vector<LevelObject*> roots;
        for (const auto& object : level.GetObjects())
        {
            if (object->GetLink().isRoot && object->GetLink().asset == levelObjectAsset && !object->IsPendingDestroy())
            {
                roots.push_back(object.get());
            }
        }
        std::vector<std::unique_ptr<Pending>> pending;
        for (LevelObject* root : roots)
        {
            auto entry = std::make_unique<Pending>();
            WriteLinkedNode(*root, entry->document, entry->document.GetAllocator());
            entry->collection = root->GetCollection();
            entry->sibling = root->GetSiblingIndex();
            entry->parentId = root->GetParent() ? root->GetParent()->GetId() : Core::Guid{};
            pending.push_back(std::move(entry));
        }
        LevelObjectLibrary::Get().Invalidate(levelObjectAsset);
        for (LevelObject* root : roots)
        {
            level.DestroyObjectImmediate(root);
        }
        for (auto& entry : pending)
        {
            LevelObject* parent = entry->parentId.IsValid() ? level.FindObject(entry->parentId) : nullptr;
            if (LevelObject* restored = ReadObject(level, entry->collection, parent, entry->document, true))
            {
                restored->SetSiblingIndex(entry->sibling);
            }
        }
    }

    void LevelSerializer::Unlink(LevelObject& linkRoot)
    {
        std::vector<LevelObject*> linked;
        std::vector<std::pair<LevelObject*, LevelObject*>> foreign;
        CollectLinkedSubtree(linkRoot, linkRoot, linked, foreign);
        for (LevelObject* object : linked)
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

    LevelObject* LevelObjectLibrary::GetPrototype(const Core::Guid& asset)
    {
        auto it = m_entries.find(asset);
        if (it != m_entries.end())
        {
            return it->second.root;
        }
        Entry entry;
        entry.level = std::make_unique<Level>("Prototype");
        entry.root = InstantiateAsset(*entry.level, asset, nullptr, nullptr, Core::Guid::Generate(), false, true);
        if (!entry.root)
        {
            return nullptr;
        }
        std::function<void(LevelObject&)> index = [&](LevelObject& object)
        {
            entry.bySource[object.GetLink().sourceId] = &object;
            for (LevelObject* child : object.GetChildren())
            {
                index(*child);
            }
        };
        index(*entry.root);
        LevelObject* root = entry.root;
        m_entries[asset] = std::move(entry);
        return root;
    }

    LevelObject* LevelObjectLibrary::FindPrototypeNode(const Core::Guid& asset, const Core::Guid& sourceId)
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
