#pragma once

#include "Core/Event.h"
#include "Core/Guid.h"
#include "RHI/Device.h"

#include <deque>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace BrightBox3D::Rendering
{
    class Renderer;
}

namespace BrightBox3D::Editor
{
    class ThumbnailCache
    {
    public:
        ThumbnailCache(RHI::Device& device, Rendering::Renderer& renderer);
        ~ThumbnailCache();

        RHI::Texture* Get(const Core::Guid& asset);
        void Update(RHI::CommandList& commands);
        void Clear();

        static constexpr uint32_t Size = 128;

    private:
        bool Generate(RHI::CommandList& commands, const Core::Guid& asset);

        RHI::Device& m_device;
        Rendering::Renderer& m_renderer;
        std::unordered_map<Core::Guid, std::unique_ptr<RHI::Texture>> m_thumbnails;
        std::unordered_set<Core::Guid> m_failed;
        std::deque<Core::Guid> m_queue;
        std::unordered_set<Core::Guid> m_queued;
        std::vector<std::unique_ptr<RHI::Texture>> m_retired;
        Core::EventHandle m_changedHandle = 0;
    };
}
