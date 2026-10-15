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

    std::shared_ptr<Asset> AssetDatabase::LoadAsset(const Core::Guid& guid)
    {
        if (!guid.IsValid())
        {
            return nullptr;
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
        std::string error;
        if (!ReadCookedPayload(record, *importer, payload) && (m_mode == AssetDatabaseMode::Cooked || !CookAsset(record, &payload, error)))
        {
            Core::Log::Error(Core::Text(Core::EngineText::AssetLoadFailed, record.path, error.empty() ? std::string(Core::Text(Core::EngineText::MissingCookedData)) : error));
            return nullptr;
        }

        std::shared_ptr<Asset> asset = importer->Load(record, payload.data(), payload.size(), error);
        if (!asset)
        {
            Core::Log::Error(Core::Text(Core::EngineText::AssetLoadFailed, record.path, error));
            return nullptr;
        }
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
