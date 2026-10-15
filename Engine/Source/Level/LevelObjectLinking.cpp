#include "Level/LevelObjectLinking.h"

#include "Level/Level.h"
#include "Level/LevelFileSchema.h"
#include "Level/LevelOverrides.h"
#include "Level/LevelSerializer.h"
#include "Level/LevelWriter.h"
#include "Serialization/ObjectSerializer.h"

#include <memory>

namespace BrightBox3D::Level
{
    using Serialization::JsonDocument;
    using Serialization::JsonValue;

    namespace
    {
        constexpr const char* AssetCopyLevelName = "AssetCopy";

        void IndexInto(LevelObject& object, LevelObjectLinking::SourceIndex& index, const LevelObject* linkRoot)
        {
            if (linkRoot && !LevelObjectLinking::IsInSameLink(object, *linkRoot))
            {
                return;
            }
            index[object.GetLink().sourceId] = &object;
            for (LevelObject* child : object.GetChildren())
            {
                IndexInto(*child, index, linkRoot);
            }
        }

        struct EditableAssetCopy
        {
            std::unique_ptr<Level> level;
            LevelObject* root = nullptr;
            LevelObjectLinking::SourceIndex bySource;
        };

        bool MakeEditableAssetCopy(const Core::Guid& asset, EditableAssetCopy& copy)
        {
            copy.level = std::make_unique<Level>(AssetCopyLevelName);
            copy.root = LevelSerializer::OpenAsset(*copy.level, nullptr, asset);
            if (!copy.root)
            {
                return false;
            }
            copy.bySource = LevelObjectLinking::IndexBySource(*copy.root);
            return true;
        }

        LevelObjectComponent* MatchingComponent(const LevelObject& object, const LevelObjectComponent* component, LevelObject& other)
        {
            return component == &object.GetTransform() ? &other.GetTransform() : other.FindComponentByLocalId(component->GetLocalId());
        }

        void CopyPseudoProperty(const LevelObject& from, const LevelObjectComponent* fromComponent, LevelObject& to, LevelObjectComponent* toComponent, const std::string& property)
        {
            JsonDocument document;
            JsonValue value;
            LevelOverrides::WritePseudoProperty(from, fromComponent, property, value, document.GetAllocator());
            LevelOverrides::ReadPseudoProperty(to, toComponent, property, value);
        }

        bool CopyValueInto(const LevelObject& liveObject, const LevelObjectComponent* liveComponent, LevelObject& targetObject, const std::string& property)
        {
            LevelObjectComponent* targetComponent = nullptr;
            if (liveComponent)
            {
                targetComponent = MatchingComponent(liveObject, liveComponent, targetObject);
                if (!targetComponent)
                {
                    return false;
                }
            }
            if (LevelOverrides::IsPseudoProperty(property))
            {
                CopyPseudoProperty(liveObject, liveComponent, targetObject, targetComponent, property);
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

        LevelObject* ReplaceLinkedInstance(LevelObject& linkRoot, const JsonDocument& document)
        {
            Level* level = linkRoot.GetLevel();
            LevelObject* parent = linkRoot.GetParent();
            LevelCollection* collection = linkRoot.GetCollection();
            int sibling = linkRoot.GetSiblingIndex();
            level->DestroyObjectImmediate(&linkRoot);
            LevelObject* restored = LevelSerializer::ReadObject(*level, collection, parent, document, Initialization::Immediate);
            if (restored)
            {
                restored->SetSiblingIndex(sibling);
            }
            return restored;
        }
    }

    bool LevelObjectLinking::IsInSameLink(const LevelObject& object, const LevelObject& linkRoot)
    {
        if (&object == &linkRoot)
        {
            return true;
        }
        if (!object.GetLink().sourceId.IsValid() || object.GetLink().isRoot)
        {
            return false;
        }
        return object.GetLinkRoot() == &linkRoot;
    }

    template <typename Object>
    void LevelObjectLinking::CollectLinkedSubtree(Object& linkRoot, Object& object, std::vector<Object*>& linked, std::vector<std::pair<Object*, Object*>>& foreign)
    {
        linked.push_back(&object);
        for (Object* child : object.GetChildren())
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

    template <typename Object>
    std::vector<Object*> LevelObjectLinking::CollectLinked(Object& linkRoot)
    {
        std::vector<Object*> linked;
        std::vector<std::pair<Object*, Object*>> foreign;
        CollectLinkedSubtree(linkRoot, linkRoot, linked, foreign);
        return linked;
    }

    template void LevelObjectLinking::CollectLinkedSubtree<LevelObject>(LevelObject&, LevelObject&, std::vector<LevelObject*>&, std::vector<std::pair<LevelObject*, LevelObject*>>&);
    template void LevelObjectLinking::CollectLinkedSubtree<const LevelObject>(const LevelObject&, const LevelObject&, std::vector<const LevelObject*>&, std::vector<std::pair<const LevelObject*, const LevelObject*>>&);
    template std::vector<LevelObject*> LevelObjectLinking::CollectLinked<LevelObject>(LevelObject&);
    template std::vector<const LevelObject*> LevelObjectLinking::CollectLinked<const LevelObject>(const LevelObject&);

    LevelObjectLinking::SourceIndex LevelObjectLinking::IndexBySource(LevelObject& root)
    {
        SourceIndex index;
        IndexInto(root, index, nullptr);
        return index;
    }

    LevelObjectLinking::SourceIndex LevelObjectLinking::IndexLinkedBySource(LevelObject& linkRoot)
    {
        SourceIndex index;
        IndexInto(linkRoot, index, &linkRoot);
        return index;
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
            other = MatchingComponent(object, component, *prototype);
            if (!other)
            {
                return;
            }
        }
        if (LevelOverrides::IsPseudoProperty(property))
        {
            CopyPseudoProperty(*prototype, other, object, component, property);
            return;
        }
        Reflection::EngineObject& live = component ? static_cast<Reflection::EngineObject&>(*component) : static_cast<Reflection::EngineObject&>(object);
        Reflection::EngineObject& reference = other ? static_cast<Reflection::EngineObject&>(*other) : static_cast<Reflection::EngineObject&>(*prototype);
        if (const Reflection::PropertyInfo* info = live.GetType().FindProperty(property))
        {
            Serialization::CopyPropertyValue(*info, reference, live);
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
        return SaveAsset(*copy.root, asset);
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
        for (LevelObject* object : LevelObjectLinking::CollectLinked(linkRoot))
        {
            LevelObject* prototype = library.FindPrototypeNode(asset, object->GetLink().sourceId);
            auto target = copy.bySource.find(object->GetLink().sourceId);
            if (!prototype || target == copy.bySource.end())
            {
                continue;
            }
            LevelOverrides::ForEachOverrideCandidate(*object, *prototype, object == &linkRoot, [&](const LevelObjectComponent* component, const LevelObjectComponent*, const std::string& property, bool equal)
            {
                if (!equal)
                {
                    CopyValueInto(*object, component, *target->second, property);
                }
            });
        }
        return SaveAsset(*copy.root, asset);
    }

    void LevelSerializer::RevertAll(LevelObject& linkRoot)
    {
        if (!linkRoot.GetLink().isRoot)
        {
            return;
        }
        JsonDocument document;
        LevelWriter::WriteLinkedNode(linkRoot, document, document.GetAllocator());
        document[LevelFileSchema::Overrides].SetArray();
        ReplaceLinkedInstance(linkRoot, document);
    }

    LevelObject* LevelSerializer::RefreshLinkedInstance(LevelObject& linkRoot)
    {
        JsonDocument document;
        LevelWriter::WriteLinkedNode(linkRoot, document, document.GetAllocator());
        return ReplaceLinkedInstance(linkRoot, document);
    }

    void LevelSerializer::RefreshLinkedInstances(Level& level, const Core::Guid& levelObjectAsset)
    {
        struct Pending
        {
            JsonDocument document;
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
            LevelWriter::WriteLinkedNode(*root, entry->document, entry->document.GetAllocator());
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
            if (LevelObject* restored = ReadObject(level, entry->collection, parent, entry->document, Initialization::Immediate))
            {
                restored->SetSiblingIndex(entry->sibling);
            }
        }
    }

    void LevelSerializer::Unlink(LevelObject& linkRoot)
    {
        for (LevelObject* object : LevelObjectLinking::CollectLinked(linkRoot))
        {
            object->ClearLink();
        }
        if (linkRoot.GetLevel())
        {
            linkRoot.GetLevel()->MarkHierarchyChanged();
        }
    }
}
