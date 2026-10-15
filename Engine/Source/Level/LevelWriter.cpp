#include "Level/LevelWriter.h"

#include "Level/Level.h"
#include "Level/LevelFileSchema.h"
#include "Level/LevelObjectLinking.h"
#include "Level/LevelOverrides.h"
#include "Level/LevelSerializer.h"
#include "Level/MissingComponent.h"
#include "Serialization/ObjectSerializer.h"

namespace BrightBox3D::Level
{
    using Serialization::JsonAllocator;
    using Serialization::JsonDocument;
    using Serialization::JsonValue;
    using Serialization::MakeString;

    namespace
    {
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
            out.AddMember(LevelFileSchema::Id, MakeString(component.GetLocalId().ToString(), allocator), allocator);
            JsonValue properties(rapidjson::kObjectType);
            if (const auto* missing = Reflection::Cast<MissingComponent>(&component))
            {
                out.AddMember(Serialization::TypeKey, MakeString(missing->GetMissingTypeName(), allocator), allocator);
                WriteRawJson(missing->GetData(), properties, allocator);
            }
            else
            {
                out.AddMember(Serialization::TypeKey, MakeString(component.GetType().GetName(), allocator), allocator);
                Serialization::SerializeProperties(component, properties, allocator);
            }
            out.AddMember(LevelFileSchema::Enabled, JsonValue(component.IsEnabled()), allocator);
            out.AddMember(LevelFileSchema::Properties, properties, allocator);
        }

        JsonValue WriteTransform(const LevelObject& object, JsonAllocator& allocator)
        {
            JsonValue transform(rapidjson::kObjectType);
            Serialization::SerializeProperties(object.GetTransform(), transform, allocator);
            return transform;
        }

        JsonValue WriteForeignChildren(const LevelObject& root, JsonAllocator& allocator)
        {
            std::vector<const LevelObject*> linked;
            std::vector<std::pair<const LevelObject*, const LevelObject*>> foreign;
            LevelObjectLinking::CollectLinkedSubtree(root, root, linked, foreign);
            JsonValue children(rapidjson::kArrayType);
            for (auto& [attachTo, child] : foreign)
            {
                if (child->GetHideInLevelContents())
                {
                    continue;
                }
                JsonValue node;
                LevelWriter::WriteNode(*child, node, allocator, LinkWriting::Preserve, NodeWriting::Level);
                node.AddMember(LevelFileSchema::AttachTo, MakeString(attachTo->GetLink().sourceId.ToString(), allocator), allocator);
                children.PushBack(node, allocator);
            }
            return children;
        }

        void WriteObjectFields(const LevelObject& object, const Core::Guid& id, JsonValue& out, JsonAllocator& allocator)
        {
            out.AddMember(LevelFileSchema::Id, MakeString(id.ToString(), allocator), allocator);
            out.AddMember(LevelFileSchema::Name, MakeString(object.GetName(), allocator), allocator);
            JsonValue properties(rapidjson::kObjectType);
            if (!object.GetMissingTypeName().empty())
            {
                out.AddMember(Serialization::TypeKey, MakeString(object.GetMissingTypeName(), allocator), allocator);
                WriteRawJson(object.GetMissingTypeData(), properties, allocator);
            }
            else
            {
                out.AddMember(Serialization::TypeKey, MakeString(object.GetType().GetName(), allocator), allocator);
                Serialization::SerializeProperties(object, properties, allocator);
            }
            out.AddMember(LevelFileSchema::Active, JsonValue(object.IsActiveSelf()), allocator);
            out.AddMember(LevelFileSchema::Properties, properties, allocator);
            out.AddMember(LevelFileSchema::Transform, WriteTransform(object, allocator), allocator);
        }

        JsonValue WriteComponents(const LevelObject& object, JsonAllocator& allocator)
        {
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
            return components;
        }

        JsonValue WriteChildren(const LevelObject& object, JsonAllocator& allocator, LinkWriting links, NodeWriting nodes)
        {
            JsonValue children(rapidjson::kArrayType);
            for (const LevelObject* child : object.GetChildren())
            {
                if (child->GetHideInLevelContents() || child->IsPendingDestroy())
                {
                    continue;
                }
                JsonValue node;
                LevelWriter::WriteNode(*child, node, allocator, nodes == NodeWriting::Asset ? LinkWriting::Flatten : links, nodes);
                children.PushBack(node, allocator);
            }
            return children;
        }
    }

    void LevelWriter::WriteLinkedNode(const LevelObject& root, JsonValue& out, JsonAllocator& allocator)
    {
        out.SetObject();
        out.AddMember(LevelFileSchema::Id, MakeString(root.GetId().ToString(), allocator), allocator);
        out.AddMember(LevelFileSchema::LevelObjectAsset, MakeString(root.GetLink().asset.ToString(), allocator), allocator);
        out.AddMember(LevelFileSchema::Name, MakeString(root.GetName(), allocator), allocator);
        out.AddMember(LevelFileSchema::Active, JsonValue(root.IsActiveSelf()), allocator);
        out.AddMember(LevelFileSchema::Transform, WriteTransform(root, allocator), allocator);
        JsonValue overrides;
        LevelOverrides::WriteOverrides(root, overrides, allocator);
        out.AddMember(LevelFileSchema::Overrides, overrides, allocator);
        out.AddMember(LevelFileSchema::Children, WriteForeignChildren(root, allocator), allocator);
    }

    void LevelWriter::WriteNode(const LevelObject& object, JsonValue& out, JsonAllocator& allocator, LinkWriting links, NodeWriting nodes)
    {
        if (object.GetMissingTypeName() == LevelFileSchema::MissingLevelObjectType)
        {
            WriteRawJson(object.GetMissingTypeData(), out, allocator);
            return;
        }
        if (links == LinkWriting::Preserve && nodes == NodeWriting::Level && object.GetLink().isRoot && object.GetLink().asset.IsValid())
        {
            WriteLinkedNode(object, out, allocator);
            return;
        }
        out.SetObject();
        Core::Guid id = nodes == NodeWriting::Asset && object.GetLink().sourceId.IsValid() ? object.GetLink().sourceId : object.GetId();
        WriteObjectFields(object, id, out, allocator);
        out.AddMember(LevelFileSchema::Components, WriteComponents(object, allocator), allocator);
        out.AddMember(LevelFileSchema::Children, WriteChildren(object, allocator, links, nodes), allocator);
    }

    void LevelSerializer::WriteObject(const LevelObject& object, JsonValue& out, JsonAllocator& allocator, LinkWriting links)
    {
        LevelWriter::WriteNode(object, out, allocator, links, NodeWriting::Level);
    }

    void LevelSerializer::WriteAssetNode(const LevelObject& root, JsonValue& out, JsonAllocator& allocator)
    {
        LevelWriter::WriteNode(root, out, allocator, LinkWriting::Flatten, NodeWriting::Asset);
    }

    void LevelSerializer::MakeLevelObjectDocument(const LevelObject& root, JsonDocument& document, Assets::AssetType type)
    {
        document.SetObject();
        auto& allocator = document.GetAllocator();
        document.AddMember(LevelFileSchema::AssetKind, JsonValue(Assets::AssetTypeName(type), allocator), allocator);
        document.AddMember(LevelFileSchema::Version, LevelFileSchema::CurrentVersion, allocator);
        JsonValue node;
        if (type == Assets::AssetType::LevelObjectVariant && root.GetLink().isRoot && root.GetLink().asset.IsValid())
        {
            LevelWriter::WriteLinkedNode(root, node, allocator);
        }
        else
        {
            WriteAssetNode(root, node, allocator);
        }
        document.AddMember(LevelFileSchema::Root, node, allocator);
    }

    void LevelSerializer::WriteEmptyCollection(const std::string& name, JsonValue& out, JsonAllocator& allocator)
    {
        out.SetObject();
        out.AddMember(LevelFileSchema::AssetKind, JsonValue(Assets::AssetTypeName(Assets::AssetType::LevelCollection), allocator), allocator);
        out.AddMember(LevelFileSchema::Version, LevelFileSchema::CurrentVersion, allocator);
        out.AddMember(LevelFileSchema::Name, MakeString(name, allocator), allocator);
        out.AddMember(LevelFileSchema::Objects, JsonValue(rapidjson::kArrayType), allocator);
    }

    void LevelSerializer::WriteCollection(const LevelCollection& collection, JsonValue& out, JsonAllocator& allocator)
    {
        WriteEmptyCollection(collection.GetName(), out, allocator);
        JsonValue& objects = out[LevelFileSchema::Objects];
        for (const LevelObject* root : collection.GetRoots())
        {
            if (root->GetHideInLevelContents() || root->IsPendingDestroy())
            {
                continue;
            }
            JsonValue node;
            LevelWriter::WriteNode(*root, node, allocator, LinkWriting::Preserve, NodeWriting::Level);
            objects.PushBack(node, allocator);
        }
    }

    void LevelSerializer::WriteLevel(const Level& level, JsonValue& out, JsonAllocator& allocator)
    {
        out.SetObject();
        out.AddMember(LevelFileSchema::AssetKind, JsonValue(Assets::AssetTypeName(Assets::AssetType::Level), allocator), allocator);
        out.AddMember(LevelFileSchema::Version, LevelFileSchema::CurrentVersion, allocator);
        JsonValue collections(rapidjson::kArrayType);
        for (const auto& collection : level.GetCollections())
        {
            JsonValue entry(rapidjson::kObjectType);
            entry.AddMember(LevelFileSchema::Collection, MakeString(collection->GetAsset().ToString(), allocator), allocator);
            entry.AddMember(LevelFileSchema::Name, MakeString(collection->GetName(), allocator), allocator);
            collections.PushBack(entry, allocator);
        }
        out.AddMember(LevelFileSchema::Collections, collections, allocator);
        JsonValue settings;
        settings.CopyFrom(level.GetSettings(), allocator);
        out.AddMember(LevelFileSchema::Settings, settings, allocator);
    }
}
