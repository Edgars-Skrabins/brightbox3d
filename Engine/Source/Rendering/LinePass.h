#pragma once

#include "Rendering/RenderPass.h"

namespace BrightBox3D::Rendering
{
    struct LineVertex;
    struct RenderView;

    class LinePass final : public RenderPass
    {
    public:
        using RenderPass::RenderPass;

        void Execute(FrameContext& frame) override;

    private:
        RHI::Pipeline* GetLinePipeline(RHI::Format format, bool depthTest);
        void DrawLines(RHI::CommandList& commands, const std::vector<LineVertex>& lines, RHI::Format format, bool depthTest, float gridFade);
    };
}
