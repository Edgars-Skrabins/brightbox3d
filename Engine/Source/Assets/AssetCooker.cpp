#include "Assets/AssetDatabase.h"

#include "Assets/AssetFileSchema.h"
#include "Core/EngineText.h"
#include "Core/FileSystem.h"
#include "Core/Hash.h"
#include "Core/Log.h"
#include "Serialization/BinaryStream.h"

#include <cstring>

namespace BrightBox3D::Assets
{
    namespace
    {
        constexpr uint32_t CookedMagic = 0x4B434242;
        constexpr uint32_t CookedFormatVersion = 1;

        struct CookedHeader
        {
            uint32_t magic = CookedMagic;
            uint32_t formatVersion = CookedFormatVersion;
            uint32_t importerVersion = 0;
            uint32_t assetType = 0;
            uint64_t sourceHash = 0;
        };
    }

    std::filesystem::path AssetDatabase::CookedPathFor(const Core::Guid& guid) const
    {
        return GetCookedDirectory() / (guid.ToString() + CookedExtension);
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
                    hash = Core::HashCombine(hash, Core::HashString(Serialization::WriteJson(*importer, Serialization::JsonFormat::Compact)));
                }
            }
        }
        return hash;
    }

    Core::Result<std::vector<uint8_t>> AssetDatabase::CookAsset(const AssetRecord& record)
    {
        AssetImporter* importer = GetImporter(record.type);
        if (!importer)
        {
            return Core::Error(Core::EngineText::NoImporter, AssetTypeName(record.type));
        }
        std::unique_ptr<Reflection::EngineObject> settings = LoadImportSettings(record);
        Core::Result<std::vector<uint8_t>> payload = importer->Cook(record, GetAbsolutePath(record), settings.get());
        if (!payload)
        {
            return payload;
        }

        uint64_t hash = ComputeSourceHash(record);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
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
        writer.WriteBytes(payload->data(), payload->size());
        if (!Core::FileSystem::WriteBinary(CookedPathFor(record.guid), writer.GetBuffer().data(), writer.GetBuffer().size()))
        {
            return Core::Error(Core::EngineText::CookedWriteFailed);
        }
        return payload;
    }

    bool AssetDatabase::ReadCookedPayload(const AssetRecord& record, const AssetImporter& importer, std::vector<uint8_t>& payload)
    {
        auto cooked = Core::FileSystem::ReadBinary(CookedPathFor(record.guid));
        if (!cooked || cooked->size() < sizeof(CookedHeader))
        {
            return false;
        }
        CookedHeader header;
        std::memcpy(&header, cooked->data(), sizeof(header));
        bool valid = header.magic == CookedMagic && header.formatVersion == CookedFormatVersion && header.importerVersion == importer.GetVersion();
        if (valid && m_mode == AssetDatabaseMode::Editor)
        {
            uint64_t hash = record.sourceHash;
            if (hash == 0)
            {
                hash = ComputeSourceHash(record);
                std::lock_guard<std::mutex> lock(m_mutex);
                m_records[record.guid].sourceHash = hash;
            }
            valid = header.sourceHash == hash;
        }
        if (!valid)
        {
            return false;
        }
        payload.assign(cooked->begin() + sizeof(CookedHeader), cooked->end());
        return true;
    }

    std::vector<Core::Error> AssetDatabase::CookAll()
    {
        std::vector<Core::Error> failures;
        for (const AssetRecord& record : GetRecords())
        {
            if (record.type == AssetType::Folder || !GetImporter(record.type))
            {
                continue;
            }
            if (Core::Result<std::vector<uint8_t>> cooked = CookAsset(record); !cooked)
            {
                failures.emplace_back(Core::EngineText::AssetCookFailed, record.path, cooked.GetError().GetText());
            }
        }
        SaveCookedManifest();
        return failures;
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
}
