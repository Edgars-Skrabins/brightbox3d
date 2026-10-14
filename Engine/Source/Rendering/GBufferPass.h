#pragma once

#include "Rendering/RenderPass.h"

namespace BrightBox3D::Rendering
{
    class GBufferPass final : public RenderPass
    {
    public:
        using RenderPass::RenderPass;

        void Execute(FrameContext& frame) override;
    };
}
