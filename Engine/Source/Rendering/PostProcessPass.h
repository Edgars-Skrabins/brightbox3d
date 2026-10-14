#pragma once

#include "Rendering/RenderPass.h"

namespace BrightBox3D::Rendering
{
    struct PostConstants;

    class PostProcessPass final : public RenderPass
    {
    public:
        using RenderPass::RenderPass;

        void Execute(FrameContext& frame) override;

    private:
        struct Exposure
        {
            RHI::Texture* texture = nullptr;
            bool automatic = false;
        };

        void BindCommon(FrameContext& frame, const PostConstants& constants);
        RHI::Texture* ResolveTemporal(FrameContext& frame);
        RHI::Texture* RenderBloom(FrameContext& frame, RHI::Texture* hdr);
        Exposure RenderExposure(FrameContext& frame, RHI::Texture* hdr);
        void RenderTonemap(FrameContext& frame, RHI::Texture* hdr, RHI::Texture* bloom, const Exposure& exposure);
    };
}
