#pragma once

#include "Rendering/RenderConstants.h"
#include "Rendering/RenderPass.h"

namespace BrightBox3D::Rendering
{
    class SSAOPass final : public RenderPass
    {
    public:
        explicit SSAOPass(const RenderPassResources& resources);

        void Execute(FrameContext& frame) override;

    private:
        float m_kernel[SSAOKernelSize][4] = {};
    };
}
