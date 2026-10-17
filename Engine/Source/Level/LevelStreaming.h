#pragma once

#include "Core/Guid.h"
#include "Core/Result.h"
#include "Level/LevelSubsystem.h"

#include <functional>
#include <future>
#include <memory>
#include <vector>

namespace BrightBox3D::Level
{
    class LevelCollection;
    class LevelCollectionAsset;

    BrightClass()
    class LevelStreaming : public LevelSubsystem
    {
        BrightBody()
    public:
        using Callback = std::function<void(LevelCollection*)>;

        explicit LevelStreaming(Level& level);
        ~LevelStreaming() override;

        void OnGameUpdate(float deltaTime) override;
        void OnGameEnd() override;

        void LoadCollectionAsync(const Core::Guid& collection, Callback callback = {});
        bool UnloadCollection(const Core::Guid& collection);
        bool IsLoaded(const Core::Guid& collection) const;
        bool IsLoading(const Core::Guid& collection) const;
        void FlushPending();

        static LevelStreaming* Get(const Level* level);
        static void LoadAsync(Level& level, const Core::Guid& collection, Callback callback = {});
        static bool Unload(Level& level, const Core::Guid& collection);

    private:
        struct Request
        {
            Core::Guid collection;
            std::future<Core::Result<std::shared_ptr<LevelCollectionAsset>>> asset;
            std::vector<Callback> callbacks;
        };

        void Complete(Request& request, const Core::Result<std::shared_ptr<LevelCollectionAsset>>& asset);

        std::vector<std::unique_ptr<Request>> m_requests;
        std::vector<Core::Guid> m_pendingRemovals;
    };
}
