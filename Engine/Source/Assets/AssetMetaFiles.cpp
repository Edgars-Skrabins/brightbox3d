#include "Assets/AssetDatabase.h"

#include "Assets/AssetFileSchema.h"
#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Reflection/TypeInfo.h"
#include "Serialization/ObjectSerializer.h"

namespace BrightBox3D::Assets
{
    std::filesystem::path AssetDatabase::MetaPathFor(const std::filesystem::path& absolutePath) const
    {
        std::filesystem::path meta = absolutePath;
        meta += MetaExtension;
        return meta;
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
                std::filesystem::path existingPath = existing != m_records.end() ? GetAbsolutePath(existing->second) : std::filesystem::path();
                if (existingPath == absolutePath || !std::filesystem::exists(existingPath))
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
}
