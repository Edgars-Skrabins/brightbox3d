#include "Level/LevelSerializer.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
#include "Core/Hash.h"
#include "Level/LevelFileSchema.h"
#include "Level/LevelReader.h"

#include <filesystem>

namespace BrightBox3D::Level
{
    using Serialization::JsonDocument;
    using Serialization::JsonValue;
    using Serialization::MakeString;

    namespace
    {
        constexpr uint64_t DeriveSaltHigh = 0xA5A5A5A5A5A5A5A5ull;
        constexpr uint64_t DeriveSaltLow = 0x5A5A5A5A5A5A5A5Aull;
        constexpr Core::Guid FallbackDerivedId{ 1, 1 };
        constexpr const char* NameSeparator = "_";

        std::string FolderPrefix(const std::string& folder)
        {
            return folder.empty() ? std::string() : folder + "/";
        }

        std::string CollectionAssetPath(const std::string& folder, const std::string& levelName, const std::string& collectionName)
        {
            return FolderPrefix(folder) + levelName + NameSeparator + collectionName + Assets::AssetTypeExtension(Assets::AssetType::LevelCollection);
        }
    }

    Core::Guid LevelSerializer::DeriveId(const Core::Guid& root, const Core::Guid& source)
    {
        Core::Guid id{ Core::HashCombine(root.high, source.high ^ DeriveSaltHigh), Core::HashCombine(root.low, source.low ^ DeriveSaltLow) };
        return id.IsValid() ? id : FallbackDerivedId;
    }

    Core::Result<LevelObject*> LevelSerializer::OpenAsset(Level& level, LevelCollection* collection, const Core::Guid& asset)
    {
        std::shared_ptr<Assets::Asset> holder;
        Core::Result<const JsonValue*> root = LevelReader::FindAssetRoot(asset, holder, TemplateAccess::Allow);
        if (!root)
        {
            return root.GetError();
        }
        LevelObject* object = ReadObject(level, collection, nullptr, **root, Initialization::Deferred);
        if (!object)
        {
            return Core::Error(Core::EngineText::LevelObjectAssetUnreadable, Assets::AssetTypeName(holder->GetAssetType()), asset.ToString());
        }
        if (!(object->GetLink().isRoot && object->GetLink().asset.IsValid()))
        {
            LevelReader::TagForEditing(*object);
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
            std::string path = database.MakeUniquePath(CollectionAssetPath(folder, collection.GetLevel().GetName(), collection.GetName()));
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

    bool LevelSerializer::SaveLevel(Level& level, SaveScope scope)
    {
        bool success = true;
        for (const auto& collection : level.GetCollections())
        {
            if (scope == SaveScope::All || collection->IsDirty() || !collection->GetAsset().IsValid())
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

    Core::Result<Core::Guid> LevelSerializer::CreateLevelAsset(const std::string& relativePath)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        std::string path = database.MakeUniquePath(relativePath);
        std::string name = std::filesystem::path(path).stem().string();
        std::string folder = std::filesystem::path(path).parent_path().generic_string();

        JsonDocument collection;
        WriteEmptyCollection(Core::Text(Core::EngineText::MainCollectionName), collection, collection.GetAllocator());
        Core::Guid collectionGuid = database.CreateJsonAsset(database.MakeUniquePath(CollectionAssetPath(folder, name, Core::Text(Core::EngineText::MainCollectionName))), collection);

        JsonDocument level;
        level.SetObject();
        auto& allocator = level.GetAllocator();
        level.AddMember(LevelFileSchema::AssetKind, JsonValue(Assets::AssetTypeName(Assets::AssetType::Level), allocator), allocator);
        level.AddMember(LevelFileSchema::Version, LevelFileSchema::CurrentVersion, allocator);
        JsonValue collections(rapidjson::kArrayType);
        JsonValue entry(rapidjson::kObjectType);
        entry.AddMember(LevelFileSchema::Collection, MakeString(collectionGuid.ToString(), allocator), allocator);
        entry.AddMember(LevelFileSchema::Name, JsonValue(Core::Text(Core::EngineText::MainCollectionName), allocator), allocator);
        collections.PushBack(entry, allocator);
        level.AddMember(LevelFileSchema::Collections, collections, allocator);
        level.AddMember(LevelFileSchema::Settings, JsonValue(rapidjson::kObjectType), allocator);
        Core::Guid guid = database.CreateJsonAsset(path, level);
        if (!guid.IsValid())
        {
            return Core::Error(Core::EngineText::LevelWriteFailed);
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
}
