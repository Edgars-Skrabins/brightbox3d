#pragma once

#include "Rendering/RenderPass.h"

namespace BrightBox3D::Rendering
{
    class LightingPass final : public RenderPass
    {
    public:
        using RenderPass::RenderPass;

        void Execute(FrameContext& frame) override;
    };
}
