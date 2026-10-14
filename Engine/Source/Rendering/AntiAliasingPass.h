#pragma once

#include "Rendering/RenderPass.h"

#include <memory>

namespace BrightBox3D::Rendering
{
    class AntiAliasingPass final : public RenderPass
    {
    public:
        using RenderPass::RenderPass;

        void Execute(FrameContext& frame) override;

    private:
        void RenderFXAA(FrameContext& frame);
        void RenderSMAA(FrameContext& frame);
        void EnsureSMAATextures();
        RHI::Pipeline* GetSMAAPipeline(ShaderProgram program, RHI::Format format);

        std::unique_ptr<RHI::Texture> m_smaaArea;
        std::unique_ptr<RHI::Texture> m_smaaSearch;
    };
}
