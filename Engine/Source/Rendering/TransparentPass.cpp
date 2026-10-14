#include "Rendering/TransparentPass.h"

#include "Rendering/FrameContext.h"

namespace BrightBox3D::Rendering
{
    void TransparentPass::Execute(FrameContext& frame)
    {
        if (frame.transparent.empty())
        {
            return;
        }
        RHI::CommandList& commands = frame.commands;
        RenderTargets& targets = frame.targets;
        RHI::RenderPassDesc pass;
        pass.colors.push_back({ targets.sceneColor.get(), 0, 0, RHI::LoadAction::Load });
        pass.depth.texture = targets.depth.get();
        pass.depth.load = RHI::LoadAction::Load;
        pass.debugName = "Transparent";
        commands.BeginRenderPass(pass);
        frame.lighting.Bind(commands, frame.viewConstants, m_resources);
        RHI::Pipeline* back = GetMeshPipeline(ShaderProgram::Forward, RHI::CullMode::Back, { SceneColorFormat }, DepthFormat, false, RHI::BlendState::Premultiplied());
        RHI::Pipeline* none = GetMeshPipeline(ShaderProgram::Forward, RHI::CullMode::None, { SceneColorFormat }, DepthFormat, false, RHI::BlendState::Premultiplied());
        DrawItems(commands, frame.transparent, back, none);
        commands.EndRenderPass();
    }
}
