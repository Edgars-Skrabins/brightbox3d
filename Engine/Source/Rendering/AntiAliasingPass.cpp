#include "Rendering/AntiAliasingPass.h"

#include "Rendering/FrameContext.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/RenderView.h"
#include "Rendering/ShaderLibrary.h"
#include "Shaders/ShaderBindings.h"

#include <AreaTex.h>
#include <SearchTex.h>

namespace BrightBox3D::Rendering
{
    void AntiAliasingPass::Execute(FrameContext& frame)
    {
        switch (frame.view.post.antiAliasing)
        {
        case AntiAliasing::FXAA:
            RenderFXAA(frame);
            break;
        case AntiAliasing::SMAA:
            RenderSMAA(frame);
            break;
        default:
            break;
        }
    }

    RHI::Pipeline* AntiAliasingPass::GetSMAAPipeline(ShaderProgram program, RHI::Format format)
    {
        RHI::PipelineDesc desc;
        desc.vertexShader = m_shaders.Get(program, RHI::ShaderStage::Vertex);
        desc.pixelShader = m_shaders.Get(program, RHI::ShaderStage::Pixel);
        desc.rasterizer.cullMode = RHI::CullMode::None;
        desc.depthStencil.depthTest = false;
        desc.depthStencil.depthWrite = false;
        desc.colorFormats = { format };
        return m_shaders.GetPipeline({ program, format }, desc);
    }

    void AntiAliasingPass::EnsureSMAATextures()
    {
        if (m_smaaArea)
        {
            return;
        }
        RHI::TextureDesc area;
        area.width = AREATEX_WIDTH;
        area.height = AREATEX_HEIGHT;
        area.format = RHI::Format::RG8Unorm;
        area.debugName = "SMAAArea";
        m_smaaArea = m_device.CreateTexture(area);
        m_device.UpdateTexture(m_smaaArea.get(), 0, 0, areaTexBytes, AREATEX_PITCH);
        RHI::TextureDesc search;
        search.width = SEARCHTEX_WIDTH;
        search.height = SEARCHTEX_HEIGHT;
        search.format = RHI::Format::R8Unorm;
        search.debugName = "SMAASearch";
        m_smaaSearch = m_device.CreateTexture(search);
        m_device.UpdateTexture(m_smaaSearch.get(), 0, 0, searchTexBytes, SEARCHTEX_PITCH);
    }

    void AntiAliasingPass::RenderFXAA(FrameContext& frame)
    {
        const RenderView& view = frame.view;
        PostConstants constants{};
        Store(constants.sourceSize, static_cast<float>(view.width), static_cast<float>(view.height), 1.0f / view.width, 1.0f / view.height);
        FullscreenPass(frame.commands, frame.output, GetFullscreenPipeline(ShaderProgram::FXAA, frame.output->GetFormat()), "FXAA", false, [&](RHI::CommandList& commands)
        {
            commands.SetConstants(BRIGHT_SLOT_VIEW_CONSTANTS, frame.viewConstants);
            commands.SetConstants(BRIGHT_SLOT_PASS_CONSTANTS, constants);
            commands.SetSampler(BRIGHT_SLOT_POST_LINEAR_SAMPLER, m_resources.GetLinearClampSampler());
            commands.SetSampler(BRIGHT_SLOT_POST_POINT_SAMPLER, m_resources.GetPointClampSampler());
            commands.SetTexture(BRIGHT_SLOT_POST_SOURCE, frame.targets.ldr.get());
        });
    }

    void AntiAliasingPass::RenderSMAA(FrameContext& frame)
    {
        EnsureSMAATextures();
        RHI::CommandList& commands = frame.commands;
        const RenderView& view = frame.view;
        RenderTargets& targets = frame.targets;
        float metrics[4] = { 1.0f / view.width, 1.0f / view.height, static_cast<float>(view.width), static_cast<float>(view.height) };
        auto beginStep = [&](RHI::Texture* target, RHI::LoadAction load, const char* name, ShaderProgram program)
        {
            RHI::RenderPassDesc pass;
            pass.colors.push_back({ target, 0, 0, load, { 0, 0, 0, 0 } });
            pass.debugName = name;
            commands.BeginRenderPass(pass);
            commands.SetPipeline(GetSMAAPipeline(program, target->GetFormat()));
            commands.SetConstants(BRIGHT_SLOT_LOCAL_CONSTANTS, metrics, sizeof(metrics));
            commands.SetSampler(BRIGHT_SLOT_SMAA_LINEAR_SAMPLER, m_resources.GetLinearClampSampler());
            commands.SetSampler(BRIGHT_SLOT_SMAA_POINT_SAMPLER, m_resources.GetPointClampSampler());
        };
        auto endStep = [&]()
        {
            commands.Draw(FullscreenTriangleVertices);
            commands.EndRenderPass();
        };

        beginStep(targets.smaaEdges.get(), RHI::LoadAction::Clear, "SMAAEdges", ShaderProgram::SMAAEdge);
        commands.SetTexture(BRIGHT_SLOT_SMAA_COLOR, targets.ldr.get());
        endStep();

        beginStep(targets.smaaBlend.get(), RHI::LoadAction::Clear, "SMAABlendWeights", ShaderProgram::SMAABlend);
        commands.SetTexture(BRIGHT_SLOT_SMAA_EDGES, targets.smaaEdges.get());
        commands.SetTexture(BRIGHT_SLOT_SMAA_AREA, m_smaaArea.get());
        commands.SetTexture(BRIGHT_SLOT_SMAA_SEARCH, m_smaaSearch.get());
        endStep();

        beginStep(frame.output, RHI::LoadAction::DontCare, "SMAANeighborhood", ShaderProgram::SMAANeighborhood);
        commands.SetTexture(BRIGHT_SLOT_SMAA_COLOR, targets.ldr.get());
        commands.SetTexture(BRIGHT_SLOT_SMAA_BLEND, targets.smaaBlend.get());
        endStep();
    }
}
