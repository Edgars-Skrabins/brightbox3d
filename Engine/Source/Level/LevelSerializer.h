#pragma once

#include "Assets/AssetType.h"
#include "Core/Guid.h"
#include "Level/Level.h"
#include "Level/LevelSerializationOptions.h"
#include "Serialization/Json.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace BrightBox3D::Level
{
    struct PropertyOverride
    {
        Core::Guid target;
        Core::Guid component;
        std::string property;
    };

    class LevelSerializer
    {
    public:
        static void WriteObject(const LevelObject& object, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator, LinkWriting links = LinkWriting::Preserve);
        static LevelObject* ReadObject(Level& level, LevelCollection* collection, LevelObject* parent, const Serialization::JsonValue& node, Initialization initialization = Initialization::Immediate);

        static void WriteEmptyCollection(const std::string& name, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator);
        static void WriteCollection(const LevelCollection& collection, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator);
        static bool ReadCollection(Level& level, LevelCollection& collection, const Serialization::JsonValue& document);

        static void WriteLevel(const Level& level, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator);
        static std::unique_ptr<Level> LoadLevel(const Core::Guid& levelAsset, std::string* error = nullptr);
        static bool SaveLevel(Level& level, SaveScope scope = SaveScope::All);
        static bool SaveCollection(LevelCollection& collection);
        static bool LoadCollectionInto(Level& level, const Core::Guid& collectionAsset, LevelCollection** loaded = nullptr);
        static Core::Guid CreateLevelAsset(const std::string& relativePath, std::string* error = nullptr);
        static std::unique_ptr<Level> CloneLevel(const Level& source);

        static LevelObject* Instantiate(Level& level, const Core::Guid& variant, LevelCollection* collection, LevelObject* parent, const Core::Guid& instanceId = {}, Initialization initialization = Initialization::Immediate);
        static LevelObject* OpenAsset(Level& level, LevelCollection* collection, const Core::Guid& asset);
        static bool SaveAsset(const LevelObject& root, const Core::Guid& asset);
        static LevelObject* InstantiateNode(Level& level, const Serialization::JsonValue& root, LevelCollection* collection, LevelObject* parent, Initialization initialization = Initialization::Immediate);

        static void WriteAssetNode(const LevelObject& root, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator);
        static void MakeLevelObjectDocument(const LevelObject& root, Serialization::JsonDocument& document, Assets::AssetType type);

        static std::vector<PropertyOverride> ComputeOverrides(const LevelObject& linkRoot);
        static bool IsPropertyOverridden(const LevelObject& object, const LevelObjectComponent* component, const std::string& property);
        static void RevertProperty(LevelObject& object, LevelObjectComponent* component, const std::string& property);
        static bool ApplyPropertyToAsset(LevelObject& object, LevelObjectComponent* component, const std::string& property);
        static bool ApplyAllToAsset(LevelObject& linkRoot);
        static void RevertAll(LevelObject& linkRoot);
        static LevelObject* RefreshLinkedInstance(LevelObject& linkRoot);
        static void RefreshLinkedInstances(Level& level, const Core::Guid& levelObjectAsset);
        static void Unlink(LevelObject& linkRoot);

        static Core::Guid DeriveId(const Core::Guid& root, const Core::Guid& source);
    };

    class LevelObjectLibrary
    {
    public:
        static LevelObjectLibrary& Get();

        LevelObject* GetPrototype(const Core::Guid& asset);
        LevelObject* FindPrototypeNode(const Core::Guid& asset, const Core::Guid& sourceId);
        void Invalidate(const Core::Guid& asset);
        void Clear();

    private:
        LevelObjectLibrary();

        struct Entry
        {
            std::unique_ptr<Level> level;
            LevelObject* root = nullptr;
            std::unordered_map<Core::Guid, LevelObject*> bySource;
        };

        std::unordered_map<Core::Guid, Entry> m_entries;
    };
}
