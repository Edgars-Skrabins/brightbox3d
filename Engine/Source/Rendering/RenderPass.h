#pragma once

#include "Rendering/RenderStatistics.h"
#include "Rendering/ShaderProgram.h"
#include "RHI/Device.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace BrightBox3D::Rendering
{
    class GpuResourceCache;
    class ShaderLibrary;
    struct DrawItem;
    struct FrameContext;

    constexpr uint32_t FullscreenTriangleVertices = 3;

    struct RenderPassResources
    {
        RHI::Device& device;
        ShaderLibrary& shaders;
        GpuResourceCache& resources;
        RenderStatistics& statistics;
    };

    class RenderPass
    {
    public:
        explicit RenderPass(const RenderPassResources& resources);
        virtual ~RenderPass() = default;

        virtual void Execute(FrameContext& frame) = 0;

    protected:
        void DrawItems(RHI::CommandList& commands, const std::vector<const DrawItem*>& items, RHI::Pipeline* single, RHI::Pipeline* doubleSided);
        void FullscreenPass(RHI::CommandList& commands, RHI::Texture* target, RHI::Pipeline* pipeline, const char* name, bool load, const std::function<void(RHI::CommandList&)>& bind);
        RHI::Pipeline* GetMeshPipeline(ShaderProgram program, RHI::CullMode cull, const std::vector<RHI::Format>& colors, RHI::Format depth, bool depthWrite, const RHI::BlendState& blend, bool depthClip = true, int32_t depthBias = 0, float slopeBias = 0.0f);
        RHI::Pipeline* GetFullscreenPipeline(ShaderProgram program, RHI::Format format, const RHI::BlendState& blend = {});

        RHI::Device& m_device;
        ShaderLibrary& m_shaders;
        GpuResourceCache& m_resources;
        RenderStatistics& m_statistics;
    };
}
