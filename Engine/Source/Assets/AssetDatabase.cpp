#include "Assets/AssetDatabase.h"

#include "Core/FileSystem.h"
#include "Core/StringUtils.h"

#include <algorithm>

namespace BrightBox3D::Assets
{
    bool AssetDatabase::IsIgnoredPath(const std::filesystem::path& path)
    {
        std::string name = path.filename().string();
        return name.empty() || name[0] == '.' || Core::EndsWith(name, MetaExtension) || Core::EndsWith(name, TempExtension);
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

    AssetRecord AssetDatabase::DescribeAsset(const std::filesystem::path& path, AssetType type, const Core::Guid& guid) const
    {
        AssetRecord record;
        record.guid = guid;
        record.path = MakeRelativePath(path);
        record.name = path.filename().string();
        record.type = type;
        if (type != AssetType::Folder)
        {
            std::error_code error;
            record.sourceTime = Core::FileSystem::LastWriteTime(path);
            record.sourceSize = static_cast<uint64_t>(std::filesystem::file_size(path, error));
        }
        return record;
    }

    bool AssetDatabase::RegisterScanned(AssetRecord record)
    {
        auto existing = m_records.find(record.guid);
        bool wasKnown = existing != m_records.end();
        bool isChanged = !wasKnown || existing->second.sourceTime != record.sourceTime || existing->second.sourceSize != record.sourceSize || existing->second.path != record.path;
        if (wasKnown && existing->second.sourceTime == record.sourceTime && existing->second.sourceSize == record.sourceSize)
        {
            record.sourceHash = existing->second.sourceHash;
        }
        Core::Guid guid = record.guid;
        RegisterRecord(std::move(record));
        if (isChanged && wasKnown)
        {
            m_loaded.erase(guid);
            return true;
        }
        return false;
    }

    std::vector<Core::Guid> AssetDatabase::ScanAssets(std::unordered_set<Core::Guid>& seen)
    {
        std::vector<Core::Guid> changed;
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
            Core::Guid guid;
            if (type == AssetType::Unknown || !ReadOrCreateMeta(path, type, guid))
            {
                continue;
            }
            seen.insert(guid);
            if (RegisterScanned(DescribeAsset(path, type, guid)))
            {
                changed.push_back(guid);
            }
        }
        return changed;
    }

    std::vector<Core::Guid> AssetDatabase::RemoveMissing(const std::unordered_set<Core::Guid>& seen)
    {
        std::vector<Core::Guid> removed;
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
        return removed;
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
            std::unordered_set<Core::Guid> seen;
            changed = ScanAssets(seen);
            removed = RemoveMissing(seen);
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
        std::string assetsSegment = std::string("/") + AssetsFolder + "/";
        for (const std::filesystem::path& path : changedPaths)
        {
            std::string text = path.generic_string();
            if (text.find(assets.generic_string()) != std::string::npos || text.find(assetsSegment) != std::string::npos)
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
}
