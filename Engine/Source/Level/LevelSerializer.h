#pragma once

#include "Core/Guid.h"
#include "Level/Level.h"
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
        static void WriteObject(const LevelObjectInstance& object, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator, bool preserveLinks = true);
        static LevelObjectInstance* ReadObject(Level& level, LevelCollection* collection, LevelObjectInstance* parent, const Serialization::JsonValue& node, bool initialize = true);

        static void WriteCollection(const LevelCollection& collection, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator);
        static bool ReadCollection(Level& level, LevelCollection& collection, const Serialization::JsonValue& document);

        static void WriteLevel(const Level& level, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator);
        static std::unique_ptr<Level> LoadLevel(const Core::Guid& levelAsset, std::string* error = nullptr);
        static bool SaveLevel(Level& level, bool onlyDirty = false);
        static bool SaveCollection(LevelCollection& collection);
        static bool LoadCollectionInto(Level& level, const Core::Guid& collectionAsset, LevelCollection** loaded = nullptr);
        static Core::Guid CreateLevelAsset(const std::string& relativePath, std::string* error = nullptr);
        static std::unique_ptr<Level> CloneLevel(const Level& source);

        static LevelObjectInstance* Instantiate(Level& level, const Core::Guid& levelObjectAsset, LevelCollection* collection, LevelObjectInstance* parent, const Core::Guid& instanceId = {}, bool initialize = true);
        static LevelObjectInstance* InstantiateNode(Level& level, const Serialization::JsonValue& root, LevelCollection* collection, LevelObjectInstance* parent, bool initialize = true);

        static void WriteAssetNode(const LevelObjectInstance& root, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator);
        static void MakeLevelObjectDocument(const LevelObjectInstance& root, Serialization::JsonDocument& document);

        static std::vector<PropertyOverride> ComputeOverrides(const LevelObjectInstance& linkRoot);
        static bool IsPropertyOverridden(const LevelObjectInstance& object, const Component* component, const std::string& property);
        static void RevertProperty(LevelObjectInstance& object, Component* component, const std::string& property);
        static bool ApplyPropertyToAsset(LevelObjectInstance& object, Component* component, const std::string& property);
        static bool ApplyAllToAsset(LevelObjectInstance& linkRoot);
        static void RevertAll(LevelObjectInstance& linkRoot);
        static LevelObjectInstance* RefreshLinkedInstance(LevelObjectInstance& linkRoot);
        static void RefreshLinkedInstances(Level& level, const Core::Guid& levelObjectAsset);
        static void Unlink(LevelObjectInstance& linkRoot);

        static Core::Guid DeriveId(const Core::Guid& root, const Core::Guid& source);
    };

    class LevelObjectLibrary
    {
    public:
        static LevelObjectLibrary& Get();

        LevelObjectInstance* GetPrototype(const Core::Guid& asset);
        LevelObjectInstance* FindPrototypeNode(const Core::Guid& asset, const Core::Guid& sourceId);
        void Invalidate(const Core::Guid& asset);
        void Clear();

    private:
        LevelObjectLibrary();

        struct Entry
        {
            std::unique_ptr<Level> level;
            LevelObjectInstance* root = nullptr;
            std::unordered_map<Core::Guid, LevelObjectInstance*> bySource;
        };

        std::unordered_map<Core::Guid, Entry> m_entries;
    };
}
