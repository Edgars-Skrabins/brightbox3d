#include "Level/LevelStreaming.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Level/Level.h"
#include "Level/LevelAssets.h"
#include "Level/LevelFileSchema.h"
#include "Level/LevelSerializer.h"

#include <chrono>

namespace BrightBox3D::Level
{
    LevelStreaming::LevelStreaming(Level& level) : LevelSubsystem(level)
    {
    }

    LevelStreaming::~LevelStreaming()
    {
        for (auto& request : m_requests)
        {
            if (request->asset.valid())
            {
                request->asset.wait();
            }
        }
    }

    LevelStreaming* LevelStreaming::Get(const Level* level)
    {
        return level ? level->GetSubsystem<LevelStreaming>() : nullptr;
    }

    void LevelStreaming::LoadAsync(Level& level, const Core::Guid& collection, Callback callback)
    {
        if (LevelStreaming* streaming = Get(&level))
        {
            streaming->LoadCollectionAsync(collection, std::move(callback));
        }
    }

    bool LevelStreaming::Unload(Level& level, const Core::Guid& collection)
    {
        LevelStreaming* streaming = Get(&level);
        return streaming && streaming->UnloadCollection(collection);
    }

    bool LevelStreaming::IsLoaded(const Core::Guid& collection) const
    {
        return GetLevel()->FindCollection(collection) != nullptr;
    }

    bool LevelStreaming::IsLoading(const Core::Guid& collection) const
    {
        for (const auto& request : m_requests)
        {
            if (request->collection == collection)
            {
                return true;
            }
        }
        return false;
    }

    void LevelStreaming::LoadCollectionAsync(const Core::Guid& collection, Callback callback)
    {
        if (LevelCollection* loaded = GetLevel()->FindCollection(collection))
        {
            if (callback)
            {
                callback(loaded);
            }
            return;
        }
        for (auto& request : m_requests)
        {
            if (request->collection == collection)
            {
                if (callback)
                {
                    request->callbacks.push_back(std::move(callback));
                }
                return;
            }
        }
        auto request = std::make_unique<Request>();
        request->collection = collection;
        if (callback)
        {
            request->callbacks.push_back(std::move(callback));
        }
        request->asset = std::async(std::launch::async, [collection]
        {
            return Assets::AssetDatabase::Get().Load<LevelCollectionAsset>(collection);
        });
        m_requests.push_back(std::move(request));
    }

    void LevelStreaming::Complete(Request& request, const Core::Result<std::shared_ptr<LevelCollectionAsset>>& asset)
    {
        LevelCollection* collection = nullptr;
        if (!asset)
        {
            Core::Log::Error(Core::Text(Core::EngineText::StreamingFailed, Assets::AssetTypeName(Assets::AssetType::LevelCollection), request.collection.ToString(), asset.GetError().GetText()));
        }
        else if (!(collection = GetLevel()->FindCollection(request.collection)))
        {
            collection = GetLevel()->AddCollection(Serialization::GetString(asset->GetDocument(), LevelFileSchema::Name, asset->GetName()), request.collection);
            LevelSerializer::ReadCollection(*GetLevel(), *collection, asset->GetDocument());
            collection->SetDirty(false);
        }
        for (Callback& callback : request.callbacks)
        {
            callback(collection);
        }
    }

    void LevelStreaming::OnGameUpdate(float deltaTime)
    {
        std::vector<Core::Guid> removals;
        removals.swap(m_pendingRemovals);
        for (const Core::Guid& guid : removals)
        {
            if (LevelCollection* collection = GetLevel()->FindCollection(guid))
            {
                GetLevel()->RemoveCollection(collection);
            }
        }
        for (size_t i = 0; i < m_requests.size();)
        {
            Request& request = *m_requests[i];
            if (request.asset.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            {
                ++i;
                continue;
            }
            std::unique_ptr<Request> finished = std::move(m_requests[i]);
            m_requests.erase(m_requests.begin() + static_cast<std::ptrdiff_t>(i));
            Complete(*finished, finished->asset.get());
        }
    }

    void LevelStreaming::FlushPending()
    {
        while (!m_requests.empty())
        {
            std::unique_ptr<Request> request = std::move(m_requests.front());
            m_requests.erase(m_requests.begin());
            Complete(*request, request->asset.get());
        }
    }

    void LevelStreaming::OnGameEnd()
    {
        for (auto& request : m_requests)
        {
            if (request->asset.valid())
            {
                request->asset.wait();
            }
        }
        m_requests.clear();
    }

    bool LevelStreaming::UnloadCollection(const Core::Guid& collection)
    {
        LevelCollection* loaded = GetLevel()->FindCollection(collection);
        if (!loaded)
        {
            return false;
        }
        if (!GetLevel()->IsPlaying())
        {
            GetLevel()->RemoveCollection(loaded);
            return true;
        }
        std::vector<LevelObject*> roots = loaded->GetRoots();
        for (LevelObject* root : roots)
        {
            GetLevel()->DestroyObject(root);
        }
        m_pendingRemovals.push_back(collection);
        return true;
    }

    namespace
    {
        struct Registration
        {
            Registration()
            {
                LevelSubsystemRegistry::Register<LevelStreaming>();
            }
        };

        Registration s_registration;
    }
}
