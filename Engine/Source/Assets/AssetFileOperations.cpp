#include "Assets/AssetDatabase.h"

#include "Assets/AssetFileSchema.h"
#include "Core/EngineText.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/StringUtils.h"
#include "Serialization/Json.h"

#include <vector>

namespace BrightBox3D::Assets
{
    namespace
    {
        void AssignNewGuid(const std::filesystem::path& metaPath)
        {
            Serialization::JsonDocument meta;
            if (!Serialization::LoadJsonFile(metaPath, meta) || !meta.IsObject())
            {
                std::error_code error;
                std::filesystem::remove(metaPath, error);
                return;
            }
            Serialization::SetMember(meta, AssetFileSchema::Guid, Serialization::MakeString(Core::Guid::Generate().ToString(), meta.GetAllocator()), meta.GetAllocator());
            Serialization::SaveJsonFile(metaPath, meta);
        }

        void AssignNewGuidsInside(const std::filesystem::path& folder)
        {
            std::vector<std::filesystem::path> metas;
            std::error_code error;
            for (std::filesystem::recursive_directory_iterator it(folder, error), end; !error && it != end; it.increment(error))
            {
                if (Core::EndsWith(it->path().filename().string(), AssetDatabase::MetaExtension))
                {
                    metas.push_back(it->path());
                }
            }
            for (const std::filesystem::path& meta : metas)
            {
                AssignNewGuid(meta);
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
            std::lock_guard<std::mutex> lock(m_mutex);
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

    Core::Guid AssetDatabase::DuplicateAsset(const Core::Guid& guid)
    {
        const AssetRecord* record = FindRecord(guid);
        if (!record || IsBuiltin(guid))
        {
            return {};
        }
        std::filesystem::path from = GetAbsolutePath(*record);
        std::string targetPath = MakeUniquePath(record->path);
        std::filesystem::path to = GetAssetsDirectory() / targetPath;
        std::error_code error;
        std::filesystem::copy(from, to, std::filesystem::copy_options::recursive, error);
        if (error)
        {
            Core::Log::Error(Core::Text(Core::EngineText::AssetWriteFailed, targetPath));
            return {};
        }
        std::filesystem::copy_file(MetaPathFor(from), MetaPathFor(to), error);
        if (!error)
        {
            AssignNewGuid(MetaPathFor(to));
        }
        if (record->type == AssetType::Folder)
        {
            AssignNewGuidsInside(to);
        }
        Refresh();
        const AssetRecord* duplicate = FindRecordByPath(targetPath);
        return duplicate ? duplicate->guid : Core::Guid{};
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
            std::lock_guard<std::mutex> lock(m_mutex);
            RemoveRecord(guid);
        }
        Refresh();
        return true;
    }
}
