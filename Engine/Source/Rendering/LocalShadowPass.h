#pragma once

#include "Math/Matrix4.h"
#include "Rendering/LocalShadowSelection.h"
#include "Rendering/RenderPass.h"
#include "Rendering/ShadowAtlasAllocator.h"

#include <memory>
#include <vector>

namespace BrightBox3D::Rendering
{
    class LocalShadowPass final : public RenderPass
    {
    public:
        using RenderPass::RenderPass;

        void Execute(FrameContext& frame) override;

    private:
        struct PlannedLight
        {
            const LocalShadowCandidate* candidate = nullptr;
            std::vector<ShadowTile> tiles;
            uint32_t firstView = 0;
        };

        void EnsureAtlas(uint32_t size);
        std::vector<PlannedLight> PlanLights(const FrameContext& frame, uint32_t atlasSize);
        void RenderTile(FrameContext& frame, const LocalShadowCandidate& candidate, const Math::Matrix4& viewProjection, const ShadowTile& tile, RHI::Pipeline* pipeline);

        std::unique_ptr<RHI::Texture> m_atlas;
        uint32_t m_atlasSize = 0;
        ShadowAtlasAllocator m_allocator;
    };
}
