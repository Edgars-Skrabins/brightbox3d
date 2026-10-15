#include "Rendering/GBufferPass.h"

#include "Rendering/FrameContext.h"
#include "Shaders/ShaderBindings.h"

namespace BrightBox3D::Rendering
{
    void GBufferPass::Execute(FrameContext& frame)
    {
        RHI::CommandList& commands = frame.commands;
        RenderTargets& targets = frame.targets;
        std::vector<RHI::Format> formats = { AlbedoFormat, NormalFormat, MaterialFormat, EmissionFormat, VelocityFormat, DepthCopyFormat };
        RHI::RenderPassDesc pass;
        pass.colors = {
            { targets.albedo.get(), 0, 0, RHI::LoadAction::Clear, { 0, 0, 0, 1 } },
            { targets.normal.get(), 0, 0, RHI::LoadAction::Clear, { 0, 1, 0, 1 } },
            { targets.material.get(), 0, 0, RHI::LoadAction::Clear, { 0, 0, 0, 0 } },
            { targets.emission.get(), 0, 0, RHI::LoadAction::Clear, { 0, 0, 0, 0 } },
            { targets.velocity.get(), 0, 0, RHI::LoadAction::Clear, { 0, 0, 0, 0 } },
            { targets.depthCopy.get(), 0, 0, RHI::LoadAction::Clear, { 1, 1, 1, 1 } }
        };
        pass.depth.texture = targets.depth.get();
        pass.depth.load = RHI::LoadAction::Clear;
        pass.debugName = "GBuffer";
        commands.BeginRenderPass(pass);
        commands.SetConstants(BRIGHT_SLOT_VIEW_CONSTANTS, frame.viewConstants);
        RHI::Pipeline* back = GetMeshPipeline(ShaderProgram::GBuffer, RHI::CullMode::Back, formats, DepthFormat, DepthWrite::On, {});
        RHI::Pipeline* none = GetMeshPipeline(ShaderProgram::GBuffer, RHI::CullMode::None, formats, DepthFormat, DepthWrite::On, {});
        DrawItems(commands, frame.opaque, back, none);
        commands.EndRenderPass();
    }
}
