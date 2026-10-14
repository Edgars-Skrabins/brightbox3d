#include "Rendering/FrameLighting.h"

#include "Rendering/GpuResourceCache.h"
#include "Rendering/RenderConstants.h"
#include "Shaders/ShaderBindings.h"

#include <algorithm>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr size_t LightHeaderSize = sizeof(LightConstants::counts) + sizeof(LightConstants::ambient);
    }

    FrameLighting::FrameLighting()
    {
        lightConstants.resize(sizeof(LightConstants));
        shadowConstants.resize(sizeof(ShadowConstants));
    }

    void FrameLighting::Bind(RHI::CommandList& commands, const ViewConstants& viewConstants, GpuResourceCache& resources) const
    {
        commands.SetConstants(BRIGHT_SLOT_VIEW_CONSTANTS, viewConstants);
        commands.SetConstants(BRIGHT_SLOT_LIGHT_CONSTANTS, lightConstants.data(), LightHeaderSize + sizeof(GpuLight) * std::max<uint32_t>(lightCount, 1));
        commands.SetConstants(BRIGHT_SLOT_SHADOW_CONSTANTS, shadowConstants.data(), shadowConstants.size());
        commands.SetTexture(BRIGHT_SLOT_SHADOW_MAP, shadowMap ? shadowMap : resources.GetDummyShadowMap());
        commands.SetSampler(BRIGHT_SLOT_SHADOW_SAMPLER, resources.GetShadowSampler());
    }
}
