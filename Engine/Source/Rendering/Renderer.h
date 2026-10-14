#pragma once

#include "Rendering/DrawCollector.h"
#include "Rendering/FrameLighting.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/RenderPass.h"
#include "Rendering/RenderStatistics.h"
#include "Rendering/RenderTargets.h"
#include "Rendering/RenderView.h"
#include "Rendering/ShaderLibrary.h"
#include "RHI/Device.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Rendering
{
    class Renderer
    {
    public:
        Renderer(RHI::Device& device, ShaderLibrary& shaders, GpuResourceCache& resources);
        ~Renderer();

        void Render(RHI::CommandList& commands, const Level::Level* level, const RenderView& view, RHI::Texture* output);
        void ReleaseView(uint64_t viewId);
        const RenderStatistics& GetStatistics() const { return m_statistics; }
        RHI::Texture* GetViewDepth(uint64_t viewId) const;

    private:
        RenderTargets& GetTargets(const RenderView& view);
        void PrepareFrame(FrameContext& frame, const Level::Level* level);

        RHI::Device& m_device;
        RenderStatistics m_statistics;
        DrawCollector m_collector;
        FrameLighting m_lighting;
        std::unordered_map<uint64_t, std::unique_ptr<RenderTargets>> m_views;
        std::vector<std::unique_ptr<RenderPass>> m_passes;
    };
}
