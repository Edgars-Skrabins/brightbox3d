#include "Rendering/LightingPass.h"

#include "Rendering/FrameContext.h"
#include "Shaders/ShaderBindings.h"

namespace BrightBox3D::Rendering
{
    void LightingPass::Execute(FrameContext& frame)
    {
        RenderTargets& targets = frame.targets;
        FullscreenPass(frame.commands, targets.sceneColor.get(), GetFullscreenPipeline(ShaderProgram::DeferredLighting, SceneColorFormat), "DeferredLighting", RHI::LoadAction::DontCare, [&](RHI::CommandList& commands)
        {
            frame.lighting.Bind(commands, frame.viewConstants, m_resources);
            commands.SetTexture(BRIGHT_SLOT_GBUFFER_ALBEDO, targets.albedo.get());
            commands.SetTexture(BRIGHT_SLOT_GBUFFER_NORMAL_ROUGHNESS, targets.normal.get());
            commands.SetTexture(BRIGHT_SLOT_GBUFFER_MATERIAL, targets.material.get());
            commands.SetTexture(BRIGHT_SLOT_GBUFFER_EMISSION, targets.emission.get());
            commands.SetTexture(BRIGHT_SLOT_GBUFFER_DEPTH, targets.depthCopy.get());
            commands.SetTexture(BRIGHT_SLOT_GBUFFER_AMBIENT_OCCLUSION, frame.ambientOcclusion);
        });
    }
}
