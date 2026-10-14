#pragma once

#include "Assets/Asset.h"
#include "Assets/AssetImporter.h"
#include "Assets/AssetType.h"
#include "Core/Event.h"
#include "Core/Guid.h"
#include "Serialization/Json.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace BrightBox3D::Assets
{
    struct AssetRecord
    {
        Core::Guid guid;
        std::string path;
        std::string name;
        AssetType type = AssetType::Unknown;
        int64_t sourceTime = 0;
        uint64_t sourceSize = 0;
        uint64_t sourceHash = 0;
    };

    enum class AssetDatabaseMode
    {
        Editor,
        Cooked
    };

    class AssetDatabase
    {
    public:
        static constexpr const char* AssetsFolder = "Assets";
        static constexpr const char* CacheFolder = "Cache";
        static constexpr const char* CookedFolder = "Cooked";
        static constexpr const char* CookedManifestFileName = "Manifest.json";
        static constexpr const char* MetaExtension = ".meta";
        static constexpr const char* TempExtension = ".tmp";
        static constexpr const char* CookedExtension = ".bin";

        static AssetDatabase& Get();
        static bool IsIgnoredPath(const std::filesystem::path& path);

        bool Open(const std::filesystem::path& projectRoot, AssetDatabaseMode mode = AssetDatabaseMode::Editor);
        void Close();
        bool IsOpen() const { return m_isOpen; }
        AssetDatabaseMode GetMode() const { return m_mode; }

        const std::filesystem::path& GetProjectRoot() const { return m_projectRoot; }
        std::filesystem::path GetAssetsDirectory() const { return m_projectRoot / AssetsFolder; }
        std::filesystem::path GetCacheDirectory() const { return m_projectRoot / CacheFolder; }
        std::filesystem::path GetCookedDirectory() const;
        std::filesystem::path GetAbsolutePath(const AssetRecord& record) const;

        void Refresh();
        void ProcessFileChanges(const std::vector<std::filesystem::path>& changedPaths);

        const AssetRecord* FindRecord(const Core::Guid& guid) const;
        const AssetRecord* FindRecordByPath(const std::string& relativePath) const;
        Core::Guid ResolveReference(const std::string& reference) const;
        std::vector<AssetRecord> GetRecords(AssetType type = AssetType::Unknown) const;
        std::string MakeRelativePath(const std::filesystem::path& absolutePath) const;

        std::shared_ptr<Asset> LoadAsset(const Core::Guid& guid);

        template <typename T>
        std::shared_ptr<T> Load(const Core::Guid& guid)
        {
            std::shared_ptr<Asset> asset = LoadAsset(guid);
            if (asset && asset->IsA(T::StaticType()))
            {
                return std::static_pointer_cast<T>(asset);
            }
            return nullptr;
        }

        std::shared_ptr<Asset> FindLoaded(const Core::Guid& guid) const;
        void Unload(const Core::Guid& guid);
        void UnloadAll();
        bool CookAsset(const AssetRecord& record, std::vector<uint8_t>* output, std::string& error);
        bool CookAll(std::string& error);

        Core::Guid ImportOrRegister(const std::filesystem::path& absolutePath);
        Core::Guid CreateJsonAsset(const std::string& relativePath, const Serialization::JsonValue& content);
        bool SaveJsonAsset(const Core::Guid& guid, const Serialization::JsonValue& content);
        bool WriteSourceFile(const Core::Guid& guid, const std::string& content);
        bool MoveAsset(const Core::Guid& guid, const std::string& newRelativePath);
        bool DeleteAsset(const Core::Guid& guid);
        std::string MakeUniquePath(const std::string& relativePath) const;

        std::unique_ptr<Reflection::EngineObject> LoadImportSettings(const AssetRecord& record) const;
        bool SaveImportSettings(const Core::Guid& guid, const Reflection::EngineObject& settings);

        using BuiltinFactory = std::function<std::shared_ptr<Asset>()>;
        void RegisterBuiltin(const Core::Guid& guid, const std::string& path, AssetType type, BuiltinFactory factory);
        bool IsBuiltin(const Core::Guid& guid) const;

        void RegisterImporter(AssetType type, std::shared_ptr<AssetImporter> importer);
        AssetImporter* GetImporter(AssetType type) const;

        Core::Event<const Core::Guid&> OnAssetChanged;
        Core::Event<const Core::Guid&> OnAssetRemoved;
        Core::Event<> OnRefreshed;
        uint64_t GetRevision() const { return m_revision; }

    private:
        AssetDatabase();

        std::filesystem::path MetaPathFor(const std::filesystem::path& absolutePath) const;
        bool ReadOrCreateMeta(const std::filesystem::path& absolutePath, AssetType type, Core::Guid& guid);
        std::filesystem::path CookedPathFor(const Core::Guid& guid) const;
        uint64_t ComputeSourceHash(const AssetRecord& record) const;
        std::unique_ptr<Reflection::EngineObject> CreateImportSettings(AssetType type) const;
        void RegisterRecord(AssetRecord record);
        AssetRecord DescribeAsset(const std::filesystem::path& path, AssetType type, const Core::Guid& guid) const;
        bool RegisterScanned(AssetRecord record);
        std::vector<Core::Guid> ScanAssets(std::unordered_set<Core::Guid>& seen);
        std::vector<Core::Guid> RemoveMissing(const std::unordered_set<Core::Guid>& seen);
        std::shared_ptr<Asset> LoadBuiltin(const Core::Guid& guid);
        bool ReadCookedPayload(const AssetRecord& record, const AssetImporter& importer, std::vector<uint8_t>& payload);
        void RemoveRecord(const Core::Guid& guid);
        void LoadCookedManifest();
        void SaveCookedManifest() const;
        void RegisterDefaultImporters();

        mutable std::recursive_mutex m_mutex;
        bool m_isOpen = false;
        AssetDatabaseMode m_mode = AssetDatabaseMode::Editor;
        std::filesystem::path m_projectRoot;
        std::unordered_map<Core::Guid, AssetRecord> m_records;
        std::unordered_map<std::string, Core::Guid> m_pathIndex;
        std::unordered_map<Core::Guid, std::shared_ptr<Asset>> m_loaded;
        std::unordered_map<int, std::shared_ptr<AssetImporter>> m_importers;
        std::unordered_map<Core::Guid, std::pair<AssetRecord, BuiltinFactory>> m_builtins;
        uint64_t m_revision = 0;
    };

    template <typename T>
    struct AssetReference
    {
        static constexpr Reflection::PropertyKind ReflectionKind = Reflection::PropertyKind::AssetReference;
        static constexpr AssetType ReflectionAssetType = T::Type;

        Core::Guid guid;

        AssetReference() = default;
        explicit AssetReference(const Core::Guid& value) : guid(value) {}

        std::shared_ptr<T> Load() const
        {
            return guid.IsValid() ? AssetDatabase::Get().Load<T>(guid) : nullptr;
        }

        bool IsSet() const { return guid.IsValid(); }
        void Clear() { guid = Core::Guid{}; }
        bool operator==(const AssetReference& other) const { return guid == other.guid; }
        bool operator!=(const AssetReference& other) const { return guid != other.guid; }
    };
}
