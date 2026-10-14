#include "Assets/AssetDatabase.h"

#include "Assets/AssetFileSchema.h"
#include "Core/EngineText.h"
#include "Core/FileSystem.h"
#include "Core/Hash.h"
#include "Core/Log.h"
#include "Core/StringUtils.h"
#include "Reflection/TypeInfo.h"
#include "Serialization/BinaryStream.h"
#include "Serialization/ObjectSerializer.h"

#include <algorithm>

namespace BrightBox3D::Assets
{
    namespace
    {
        constexpr uint32_t CookedMagic = 0x4B434242;
        constexpr uint32_t CookedFormatVersion = 1;
        constexpr const char* MetaExtension = ".meta";

        struct CookedHeader
        {
            uint32_t magic = CookedMagic;
            uint32_t formatVersion = CookedFormatVersion;
            uint32_t importerVersion = 0;
            uint32_t assetType = 0;
            uint64_t sourceHash = 0;
        };
    }

    bool AssetDatabase::IsIgnoredPath(const std::filesystem::path& path)
    {
        std::string name = path.filename().string();
        return name.empty() || name[0] == '.' || Core::EndsWith(name, MetaExtension) || Core::EndsWith(name, ".tmp");
    }

    AssetDatabase& AssetDatabase::Get()
    {
        static AssetDatabase* database = new AssetDatabase();
        return *database;
    }

    AssetDatabase::AssetDatabase()
    {
        RegisterDefaultImporters();
    }

    void AssetDatabase::RegisterDefaultImporters()
    {
        auto json = std::make_shared<JsonAssetImporter>();
        auto text = std::make_shared<TextAssetImporter>();
        RegisterImporter(AssetType::Screen, text);
        RegisterImporter(AssetType::Widget, text);
        RegisterImporter(AssetType::StyleSheet, text);
        RegisterImporter(AssetType::Material, json);
    }

    void AssetDatabase::RegisterBuiltin(const Core::Guid& guid, const std::string& path, AssetType type, BuiltinFactory factory)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        AssetRecord record;
        record.guid = guid;
        record.path = path;
        record.name = std::filesystem::path(path).filename().string();
        record.type = type;
        m_builtins[guid] = { record, std::move(factory) };
    }

    bool AssetDatabase::IsBuiltin(const Core::Guid& guid) const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_builtins.count(guid) > 0;
    }

    void AssetDatabase::RegisterImporter(AssetType type, std::shared_ptr<AssetImporter> importer)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_importers[static_cast<int>(type)] = std::move(importer);
    }

    AssetImporter* AssetDatabase::GetImporter(AssetType type) const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_importers.find(static_cast<int>(type));
        return it == m_importers.end() ? nullptr : it->second.get();
    }

    bool AssetDatabase::Open(const std::filesystem::path& projectRoot, AssetDatabaseMode mode)
    {
        Close();
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::error_code error;
        m_projectRoot = std::filesystem::weakly_canonical(projectRoot, error);
        if (error)
        {
            m_projectRoot = projectRoot;
        }
        m_mode = mode;
        m_isOpen = true;
        if (mode == AssetDatabaseMode::Cooked)
        {
            LoadCookedManifest();
        }
        else
        {
            Core::FileSystem::EnsureDirectory(GetAssetsDirectory());
            Core::FileSystem::EnsureDirectory(GetCookedDirectory());
            Refresh();
        }
        return true;
    }

    void AssetDatabase::Close()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto it = m_loaded.begin(); it != m_loaded.end();)
        {
            it = m_builtins.count(it->first) ? std::next(it) : m_loaded.erase(it);
        }
        m_records.clear();
        m_pathIndex.clear();
        m_isOpen = false;
        m_projectRoot.clear();
        ++m_revision;
    }

    std::filesystem::path AssetDatabase::GetCookedDirectory() const
    {
        return m_mode == AssetDatabaseMode::Cooked ? m_projectRoot / CookedFolder : GetCacheDirectory() / CookedFolder;
    }

    std::filesystem::path AssetDatabase::GetAbsolutePath(const AssetRecord& record) const
    {
        return GetAssetsDirectory() / std::filesystem::path(record.path);
    }

    std::filesystem::path AssetDatabase::MetaPathFor(const std::filesystem::path& absolutePath) const
    {
        std::filesystem::path meta = absolutePath;
        meta += MetaExtension;
        return meta;
    }

    std::filesystem::path AssetDatabase::CookedPathFor(const Core::Guid& guid) const
    {
        return GetCookedDirectory() / (guid.ToString() + ".bin");
    }

    std::string AssetDatabase::MakeRelativePath(const std::filesystem::path& absolutePath) const
    {
        std::error_code error;
        std::filesystem::path relative = std::filesystem::relative(absolutePath, GetAssetsDirectory(), error);
        if (error)
        {
            return {};
        }
        return relative.generic_string();
    }

    bool AssetDatabase::ReadOrCreateMeta(const std::filesystem::path& absolutePath, AssetType type, Core::Guid& guid)
    {
        std::filesystem::path metaPath = MetaPathFor(absolutePath);
        Serialization::JsonDocument meta;
        if (std::filesystem::exists(metaPath) && Serialization::LoadJsonFile(metaPath, meta) && meta.IsObject())
        {
            guid = Core::Guid::Parse(Serialization::GetString(meta, AssetFileSchema::Guid));
            if (guid.IsValid() && !m_records.count(guid))
            {
                return true;
            }
            if (guid.IsValid())
            {
                auto existing = m_records.find(guid);
                if (existing != m_records.end() && GetAbsolutePath(existing->second) == absolutePath)
                {
                    return true;
                }
                Core::Log::Warning(Core::Text(Core::EngineText::DuplicateAssetGuid, metaPath.generic_string()));
            }
        }
        else
        {
            meta.SetObject();
        }

        guid = Core::Guid::Generate();
        auto& allocator = meta.GetAllocator();
        Serialization::SetMember(meta, AssetFileSchema::Guid, Serialization::MakeString(guid.ToString(), allocator), allocator);
        Serialization::SetMember(meta, AssetFileSchema::Type, Serialization::MakeString(AssetTypeName(type), allocator), allocator);
        if (!Serialization::FindMember(meta, AssetFileSchema::Importer))
        {
            Serialization::JsonValue settings(rapidjson::kObjectType);
            if (std::unique_ptr<Reflection::EngineObject> defaults = CreateImportSettings(type))
            {
                Serialization::SerializeProperties(*defaults, settings, allocator);
            }
            Serialization::SetMember(meta, AssetFileSchema::Importer, std::move(settings), allocator);
        }
        return Serialization::SaveJsonFile(metaPath, meta);
    }

    void AssetDatabase::RegisterRecord(AssetRecord record)
    {
        auto existing = m_records.find(record.guid);
        if (existing != m_records.end())
        {
            m_pathIndex.erase(existing->second.path);
        }
        m_pathIndex[record.path] = record.guid;
        m_records[record.guid] = std::move(record);
    }

    void AssetDatabase::RemoveRecord(const Core::Guid& guid)
    {
        auto it = m_records.find(guid);
        if (it == m_records.end())
        {
            return;
        }
        m_pathIndex.erase(it->second.path);
        m_records.erase(it);
        m_loaded.erase(guid);
        std::error_code error;
        std::filesystem::remove(CookedPathFor(guid), error);
    }

    void AssetDatabase::Refresh()
    {
        std::vector<Core::Guid> changed;
        std::vector<Core::Guid> removed;
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            if (!m_isOpen || m_mode != AssetDatabaseMode::Editor)
            {
                return;
            }

            std::unordered_map<Core::Guid, bool> seen;
            std::error_code error;
            std::filesystem::recursive_directory_iterator iterator(GetAssetsDirectory(), std::filesystem::directory_options::skip_permission_denied, error);
            for (; !error && iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(error))
            {
                const std::filesystem::path& path = iterator->path();
                if (IsIgnoredPath(path))
                {
                    if (iterator->is_directory() && path.filename().string().starts_with("."))
                    {
                        iterator.disable_recursion_pending();
                    }
                    continue;
                }
                AssetType type = iterator->is_directory() ? AssetType::Folder : AssetTypeFromPath(path);
                if (type == AssetType::Unknown)
                {
                    continue;
                }
                Core::Guid guid;
                if (!ReadOrCreateMeta(path, type, guid))
                {
                    continue;
                }
                seen[guid] = true;

                AssetRecord record;
                record.guid = guid;
                record.path = MakeRelativePath(path);
                record.name = path.filename().string();
                record.type = type;
                if (type != AssetType::Folder)
                {
                    record.sourceTime = Core::FileSystem::LastWriteTime(path);
                    record.sourceSize = static_cast<uint64_t>(std::filesystem::file_size(path, error));
                    error.clear();
                }

                auto existing = m_records.find(guid);
                bool isChanged = existing == m_records.end() || existing->second.sourceTime != record.sourceTime || existing->second.sourceSize != record.sourceSize || existing->second.path != record.path;
                if (existing != m_records.end() && existing->second.sourceTime == record.sourceTime && existing->second.sourceSize == record.sourceSize)
                {
                    record.sourceHash = existing->second.sourceHash;
                }
                bool wasKnown = existing != m_records.end();
                RegisterRecord(record);
                if (isChanged && wasKnown)
                {
                    m_loaded.erase(guid);
                    changed.push_back(guid);
                }
            }

            for (auto it = m_records.begin(); it != m_records.end();)
            {
                if (!seen.count(it->first))
                {
                    removed.push_back(it->first);
                    m_pathIndex.erase(it->second.path);
                    m_loaded.erase(it->first);
                    it = m_records.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            ++m_revision;
        }

        for (const Core::Guid& guid : changed)
        {
            OnAssetChanged.Broadcast(guid);
        }
        for (const Core::Guid& guid : removed)
        {
            OnAssetRemoved.Broadcast(guid);
        }
        OnRefreshed.Broadcast();
    }

    void AssetDatabase::ProcessFileChanges(const std::vector<std::filesystem::path>& changedPaths)
    {
        bool relevant = false;
        std::filesystem::path assets = GetAssetsDirectory();
        for (const std::filesystem::path& path : changedPaths)
        {
            std::string text = path.generic_string();
            if (text.find(assets.generic_string()) != std::string::npos || text.find("/Assets/") != std::string::npos)
            {
                relevant = true;
                break;
            }
        }
        if (relevant)
        {
            Refresh();
        }
    }

    const AssetRecord* AssetDatabase::FindRecord(const Core::Guid& guid) const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_records.find(guid);
        if (it != m_records.end())
        {
            return &it->second;
        }
        auto builtin = m_builtins.find(guid);
        return builtin == m_builtins.end() ? nullptr : &builtin->second.first;
    }

    const AssetRecord* AssetDatabase::FindRecordByPath(const std::string& relativePath) const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_pathIndex.find(std::filesystem::path(relativePath).generic_string());
        return it == m_pathIndex.end() ? nullptr : FindRecord(it->second);
    }

    Core::Guid AssetDatabase::ResolveReference(const std::string& reference) const
    {
        Core::Guid guid = Core::Guid::Parse(reference);
        if (guid.IsValid())
        {
            return guid;
        }
        std::string prefix = std::string(AssetsFolder) + '/';
        std::string path = Core::StartsWith(reference, prefix) ? reference.substr(prefix.size()) : reference;
        const AssetRecord* record = FindRecordByPath(path);
        return record ? record->guid : Core::Guid{};
    }

    std::vector<AssetRecord> AssetDatabase::GetRecords(AssetType type) const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<AssetRecord> result;
        for (const auto& [guid, record] : m_records)
        {
            if (type == AssetType::Unknown || record.type == type)
            {
                result.push_back(record);
            }
        }
        for (const auto& [guid, entry] : m_builtins)
        {
            if (type != AssetType::Unknown && entry.first.type == type)
            {
                result.push_back(entry.first);
            }
        }
        std::sort(result.begin(), result.end(), [](const AssetRecord& a, const AssetRecord& b) { return a.path < b.path; });
        return result;
    }

    std::unique_ptr<Reflection::EngineObject> AssetDatabase::CreateImportSettings(AssetType type) const
    {
        AssetImporter* importer = GetImporter(type);
        const Reflection::TypeInfo* settingsType = importer ? importer->GetSettingsType() : nullptr;
        return settingsType ? std::unique_ptr<Reflection::EngineObject>(settingsType->CreateInstance()) : nullptr;
    }

    std::unique_ptr<Reflection::EngineObject> AssetDatabase::LoadImportSettings(const AssetRecord& record) const
    {
        std::unique_ptr<Reflection::EngineObject> settings = CreateImportSettings(record.type);
        if (!settings || m_mode == AssetDatabaseMode::Cooked)
        {
            return settings;
        }
        Serialization::JsonDocument meta;
        if (!Serialization::LoadJsonFile(MetaPathFor(GetAbsolutePath(record)), meta))
        {
            return settings;
        }
        if (const Serialization::JsonValue* importer = Serialization::FindMember(meta, AssetFileSchema::Importer))
        {
            Serialization::DeserializeProperties(*settings, *importer);
        }
        return settings;
    }

    bool AssetDatabase::SaveImportSettings(const Core::Guid& guid, const Reflection::EngineObject& settings)
    {
        const AssetRecord* record = FindRecord(guid);
        if (!record)
        {
            return false;
        }
        std::filesystem::path metaPath = MetaPathFor(GetAbsolutePath(*record));
        Serialization::JsonDocument meta;
        if (!Serialization::LoadJsonFile(metaPath, meta) || !meta.IsObject())
        {
            return false;
        }
        Serialization::JsonValue importer;
        if (const Serialization::JsonValue* existing = Serialization::FindMember(meta, AssetFileSchema::Importer))
        {
            importer.CopyFrom(*existing, meta.GetAllocator());
        }
        Serialization::SerializeProperties(settings, importer, meta.GetAllocator());
        Serialization::SetMember(meta, AssetFileSchema::Importer, std::move(importer), meta.GetAllocator());
        if (!Serialization::SaveJsonFile(metaPath, meta))
        {
            return false;
        }
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_records[guid].sourceHash = 0;
            m_loaded.erase(guid);
        }
        OnAssetChanged.Broadcast(guid);
        return true;
    }

    uint64_t AssetDatabase::ComputeSourceHash(const AssetRecord& record) const
    {
        auto bytes = Core::FileSystem::ReadBinary(GetAbsolutePath(record));
        uint64_t hash = bytes ? Core::HashBytes(bytes->data(), bytes->size()) : 0;
        auto meta = Core::FileSystem::ReadText(MetaPathFor(GetAbsolutePath(record)));
        if (meta)
        {
            Serialization::JsonDocument document;
            if (Serialization::ParseJson(*meta, document))
            {
                if (const Serialization::JsonValue* importer = Serialization::FindMember(document, AssetFileSchema::Importer))
                {
                    hash = Core::HashCombine(hash, Core::HashString(Serialization::WriteJson(*importer, false)));
                }
            }
        }
        return hash;
    }

    bool AssetDatabase::CookAsset(const AssetRecord& record, std::vector<uint8_t>* output, std::string& error)
    {
        AssetImporter* importer = GetImporter(record.type);
        if (!importer)
        {
            error = Core::Text(Core::EngineText::NoImporter, AssetTypeName(record.type));
            return false;
        }
        std::unique_ptr<Reflection::EngineObject> settings = LoadImportSettings(record);
        std::vector<uint8_t> payload;
        if (!importer->Cook(record, GetAbsolutePath(record), settings.get(), payload, error))
        {
            return false;
        }

        uint64_t hash = ComputeSourceHash(record);
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            auto it = m_records.find(record.guid);
            if (it != m_records.end())
            {
                it->second.sourceHash = hash;
            }
        }

        CookedHeader header;
        header.importerVersion = importer->GetVersion();
        header.assetType = static_cast<uint32_t>(record.type);
        header.sourceHash = hash;
        Serialization::BinaryWriter writer;
        writer.Write(header);
        writer.WriteBytes(payload.data(), payload.size());
        if (!Core::FileSystem::WriteBinary(CookedPathFor(record.guid), writer.GetBuffer().data(), writer.GetBuffer().size()))
        {
            error = Core::Text(Core::EngineText::CookedWriteFailed);
            return false;
        }
        if (output)
        {
            *output = std::move(payload);
        }
        return true;
    }

    std::shared_ptr<Asset> AssetDatabase::LoadAsset(const Core::Guid& guid)
    {
        if (!guid.IsValid())
        {
            return nullptr;
        }
        AssetRecord record;
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            auto loaded = m_loaded.find(guid);
            if (loaded != m_loaded.end())
            {
                return loaded->second;
            }
            auto builtin = m_builtins.find(guid);
            if (builtin != m_builtins.end())
            {
                std::shared_ptr<Asset> asset = builtin->second.second();
                if (asset)
                {
                    const AssetRecord& builtinRecord = builtin->second.first;
                    std::string name = builtinRecord.name;
                    size_t dot = name.find('.');
                    asset->SetIdentity(guid, dot == std::string::npos ? name : name.substr(0, dot), builtinRecord.path, builtinRecord.type);
                    m_loaded[guid] = asset;
                }
                return asset;
            }
            auto it = m_records.find(guid);
            if (it == m_records.end())
            {
                return nullptr;
            }
            record = it->second;
        }

        AssetImporter* importer = GetImporter(record.type);
        if (!importer)
        {
            return nullptr;
        }

        std::vector<uint8_t> payload;
        bool havePayload = false;
        auto cooked = Core::FileSystem::ReadBinary(CookedPathFor(guid));
        if (cooked && cooked->size() >= sizeof(CookedHeader))
        {
            CookedHeader header;
            std::memcpy(&header, cooked->data(), sizeof(header));
            bool valid = header.magic == CookedMagic && header.formatVersion == CookedFormatVersion && header.importerVersion == importer->GetVersion();
            if (valid && m_mode == AssetDatabaseMode::Editor)
            {
                uint64_t hash = record.sourceHash;
                if (hash == 0)
                {
                    hash = ComputeSourceHash(record);
                    std::lock_guard<std::recursive_mutex> lock(m_mutex);
                    m_records[guid].sourceHash = hash;
                }
                valid = header.sourceHash == hash;
            }
            if (valid)
            {
                payload.assign(cooked->begin() + sizeof(CookedHeader), cooked->end());
                havePayload = true;
            }
        }

        std::string error;
        if (!havePayload)
        {
            if (m_mode == AssetDatabaseMode::Cooked || !CookAsset(record, &payload, error))
            {
                Core::Log::Error(Core::Text(Core::EngineText::AssetLoadFailed, record.path, error.empty() ? std::string(Core::Text(Core::EngineText::MissingCookedData)) : error));
                return nullptr;
            }
        }

        std::shared_ptr<Asset> asset = importer->Load(record, payload.data(), payload.size(), error);
        if (!asset)
        {
            Core::Log::Error(Core::Text(Core::EngineText::AssetLoadFailed, record.path, error));
            return nullptr;
        }
        std::filesystem::path path(record.path);
        std::string name = path.filename().string();
        size_t dot = name.find('.');
        asset->SetIdentity(guid, dot == std::string::npos ? name : name.substr(0, dot), record.path, record.type);

        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_loaded[guid] = asset;
        return asset;
    }

    std::shared_ptr<Asset> AssetDatabase::FindLoaded(const Core::Guid& guid) const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_loaded.find(guid);
        return it == m_loaded.end() ? nullptr : it->second;
    }

    void AssetDatabase::Unload(const Core::Guid& guid)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_loaded.erase(guid);
    }

    void AssetDatabase::UnloadAll()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto it = m_loaded.begin(); it != m_loaded.end();)
        {
            it = m_builtins.count(it->first) ? std::next(it) : m_loaded.erase(it);
        }
    }

    bool AssetDatabase::CookAll(std::string& error)
    {
        bool success = true;
        for (const AssetRecord& record : GetRecords())
        {
            if (record.type == AssetType::Folder || !GetImporter(record.type))
            {
                continue;
            }
            std::string assetError;
            if (!CookAsset(record, nullptr, assetError))
            {
                error += record.path + ": " + assetError + "\n";
                success = false;
            }
        }
        SaveCookedManifest();
        return success;
    }

    void AssetDatabase::SaveCookedManifest() const
    {
        Serialization::JsonDocument manifest;
        manifest.SetArray();
        auto& allocator = manifest.GetAllocator();
        for (const AssetRecord& record : GetRecords())
        {
            Serialization::JsonValue entry(rapidjson::kObjectType);
            entry.AddMember(AssetFileSchema::Guid, Serialization::MakeString(record.guid.ToString(), allocator), allocator);
            entry.AddMember(AssetFileSchema::Path, Serialization::MakeString(record.path, allocator), allocator);
            entry.AddMember(AssetFileSchema::Type, Serialization::MakeString(AssetTypeName(record.type), allocator), allocator);
            manifest.PushBack(entry, allocator);
        }
        Serialization::SaveJsonFile(GetCookedDirectory() / CookedManifestFileName, manifest);
    }

    void AssetDatabase::LoadCookedManifest()
    {
        Serialization::JsonDocument manifest;
        if (!Serialization::LoadJsonFile(GetCookedDirectory() / CookedManifestFileName, manifest) || !manifest.IsArray())
        {
            Core::Log::Error(Core::Text(Core::EngineText::CookedManifestMissing));
            return;
        }
        for (const Serialization::JsonValue& entry : manifest.GetArray())
        {
            AssetRecord record;
            record.guid = Core::Guid::Parse(Serialization::GetString(entry, AssetFileSchema::Guid));
            record.path = Serialization::GetString(entry, AssetFileSchema::Path);
            record.name = std::filesystem::path(record.path).filename().string();
            record.type = AssetTypeFromName(Serialization::GetString(entry, AssetFileSchema::Type));
            if (record.guid.IsValid())
            {
                RegisterRecord(record);
            }
        }
    }

    Core::Guid AssetDatabase::ImportOrRegister(const std::filesystem::path& absolutePath)
    {
        Refresh();
        const AssetRecord* record = FindRecordByPath(MakeRelativePath(absolutePath));
        return record ? record->guid : Core::Guid{};
    }

    std::string AssetDatabase::MakeUniquePath(const std::string& relativePath) const
    {
        std::filesystem::path path(relativePath);
        std::string filename = path.filename().string();
        size_t dot = filename.find('.');
        std::string stem = dot == std::string::npos ? filename : filename.substr(0, dot);
        std::string extension = dot == std::string::npos ? "" : filename.substr(dot);
        std::filesystem::path candidate = path;
        for (int index = 1; std::filesystem::exists(GetAssetsDirectory() / candidate); ++index)
        {
            candidate = path.parent_path() / (stem + " " + std::to_string(index) + extension);
        }
        return candidate.generic_string();
    }

    Core::Guid AssetDatabase::CreateJsonAsset(const std::string& relativePath, const Serialization::JsonValue& content)
    {
        std::filesystem::path absolute = GetAssetsDirectory() / relativePath;
        if (!Serialization::SaveJsonFile(absolute, content))
        {
            Core::Log::Error(Core::Text(Core::EngineText::AssetWriteFailed, relativePath));
            return {};
        }
        return ImportOrRegister(absolute);
    }

    bool AssetDatabase::SaveJsonAsset(const Core::Guid& guid, const Serialization::JsonValue& content)
    {
        return WriteSourceFile(guid, Serialization::WriteJson(content, true));
    }

    bool AssetDatabase::WriteSourceFile(const Core::Guid& guid, const std::string& content)
    {
        AssetRecord record;
        {
            const AssetRecord* found = FindRecord(guid);
            if (!found)
            {
                return false;
            }
            record = *found;
        }
        if (!Core::FileSystem::WriteText(GetAbsolutePath(record), content))
        {
            return false;
        }
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            AssetRecord& stored = m_records[guid];
            stored.sourceTime = Core::FileSystem::LastWriteTime(GetAbsolutePath(record));
            std::error_code error;
            stored.sourceSize = static_cast<uint64_t>(std::filesystem::file_size(GetAbsolutePath(record), error));
            stored.sourceHash = 0;
            m_loaded.erase(guid);
            ++m_revision;
        }
        OnAssetChanged.Broadcast(guid);
        return true;
    }

    bool AssetDatabase::MoveAsset(const Core::Guid& guid, const std::string& newRelativePath)
    {
        const AssetRecord* record = FindRecord(guid);
        if (!record)
        {
            return false;
        }
        std::filesystem::path from = GetAbsolutePath(*record);
        std::filesystem::path to = GetAssetsDirectory() / newRelativePath;
        if (std::filesystem::exists(to))
        {
            return false;
        }
        std::error_code error;
        std::filesystem::create_directories(to.parent_path(), error);
        std::filesystem::rename(from, to, error);
        if (error)
        {
            return false;
        }
        std::filesystem::rename(MetaPathFor(from), MetaPathFor(to), error);
        Refresh();
        return true;
    }

    bool AssetDatabase::DeleteAsset(const Core::Guid& guid)
    {
        const AssetRecord* record = FindRecord(guid);
        if (!record)
        {
            return false;
        }
        std::filesystem::path path = GetAbsolutePath(*record);
        std::error_code error;
        if (record->type == AssetType::Folder)
        {
            std::filesystem::remove_all(path, error);
        }
        else
        {
            std::filesystem::remove(path, error);
        }
        std::filesystem::remove(MetaPathFor(path), error);
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            RemoveRecord(guid);
        }
        Refresh();
        return true;
    }
}
