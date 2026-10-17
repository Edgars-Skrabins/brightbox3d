#include "Assets/AssetDatabase.h"

#include "Core/EngineText.h"
#include "Core/Log.h"

namespace BrightBox3D::Assets
{
    namespace
    {
        std::string AssetStem(const std::string& name)
        {
            size_t dot = name.find('.');
            return dot == std::string::npos ? name : name.substr(0, dot);
        }
    }

    std::shared_ptr<Asset> AssetDatabase::LoadBuiltin(const Core::Guid& guid)
    {
        auto& [record, factory] = m_builtins.at(guid);
        std::shared_ptr<Asset> asset = factory();
        if (asset)
        {
            asset->SetIdentity(guid, AssetStem(record.name), record.path, record.type);
            m_loaded[guid] = asset;
        }
        return asset;
    }

    bool IsAbsentAsset(const Core::Error& error)
    {
        return error.GetId() == Core::EngineText::AssetNotSet || error.GetId() == Core::EngineText::AssetNotFound;
    }

    void ReportLoadFailure(const Core::Error& error)
    {
        if (!IsAbsentAsset(error))
        {
            Core::Log::Error(error.GetText());
        }
    }

    Core::Result<std::shared_ptr<Asset>> AssetDatabase::LoadAsset(const Core::Guid& guid)
    {
        if (!guid.IsValid())
        {
            return Core::Error(Core::EngineText::AssetNotSet);
        }
        AssetRecord record;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (auto loaded = m_loaded.find(guid); loaded != m_loaded.end())
            {
                return loaded->second;
            }
            if (m_builtins.count(guid))
            {
                return LoadBuiltin(guid);
            }
            auto it = m_records.find(guid);
            if (it == m_records.end())
            {
                return Core::Error(Core::EngineText::AssetNotFound, guid.ToString());
            }
            record = it->second;
        }

        AssetImporter* importer = GetImporter(record.type);
        if (!importer)
        {
            return Core::Error(Core::EngineText::AssetLoadFailed, record.path, Core::Text(Core::EngineText::NoImporter, AssetTypeName(record.type)));
        }
        std::vector<uint8_t> payload;
        if (!ReadCookedPayload(record, *importer, payload))
        {
            if (m_mode == AssetDatabaseMode::Cooked)
            {
                return Core::Error(Core::EngineText::AssetLoadFailed, record.path, Core::Text(Core::EngineText::MissingCookedData));
            }
            Core::Result<std::vector<uint8_t>> cooked = CookAsset(record);
            if (!cooked)
            {
                return Core::Error(Core::EngineText::AssetLoadFailed, record.path, cooked.GetError().GetText());
            }
            payload = std::move(*cooked);
        }

        Core::Result<std::shared_ptr<Asset>> loaded = importer->Load(record, payload.data(), payload.size());
        if (!loaded)
        {
            return Core::Error(Core::EngineText::AssetLoadFailed, record.path, loaded.GetError().GetText());
        }
        std::shared_ptr<Asset> asset = std::move(*loaded);
        asset->SetIdentity(guid, AssetStem(std::filesystem::path(record.path).filename().string()), record.path, record.type);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loaded[guid] = asset;
        return asset;
    }

    std::shared_ptr<Asset> AssetDatabase::FindLoaded(const Core::Guid& guid) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_loaded.find(guid);
        return it == m_loaded.end() ? nullptr : it->second;
    }

    void AssetDatabase::Unload(const Core::Guid& guid)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loaded.erase(guid);
    }

    void AssetDatabase::UnloadAll()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_loaded.begin(); it != m_loaded.end();)
        {
            it = m_builtins.count(it->first) ? std::next(it) : m_loaded.erase(it);
        }
    }
}
