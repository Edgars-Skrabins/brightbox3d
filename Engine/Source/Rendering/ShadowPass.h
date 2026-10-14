#pragma once

#include "Math/Matrix4.h"
#include "Rendering/RenderPass.h"

#include <memory>

namespace BrightBox3D::Rendering
{
    struct RenderView;

    class ShadowPass final : public RenderPass
    {
    public:
        using RenderPass::RenderPass;

        void Execute(FrameContext& frame) override;

    private:
        struct Cascade
        {
            Math::Matrix4 shadowMatrix;
            float radius = 0.0f;
        };

        void EnsureShadowMap(uint32_t resolution);
        Cascade FitCascade(const RenderView& view, const Math::Vector3& lightDirection, float sliceNear, float sliceFar, uint32_t resolution) const;
        void RenderCascade(FrameContext& frame, uint32_t cascade, const Math::Matrix4& shadowMatrix, RHI::Pipeline* pipeline);

        std::unique_ptr<RHI::Texture> m_shadowMap;
        uint32_t m_shadowResolution = 0;
    };
}
