#include "Rendering/LinePass.h"

#include "Rendering/DebugDraw.h"
#include "Rendering/FrameContext.h"
#include "Rendering/RenderView.h"
#include "Rendering/ShaderLibrary.h"
#include "Shaders/ShaderBindings.h"

#include <cmath>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr float GridSpacing = 1.0f;
        constexpr int GridExtent = 100;
        constexpr int GridMajorInterval = 10;
        constexpr float GridHeight = 0.01f;
        constexpr float GridFadeDistance = 80.0f;
        constexpr float NoFade = 0.0f;

        RHI::VertexLayout LineLayout()
        {
            RHI::VertexLayout layout;
            layout.attributes = {
                { "POSITION", 0, RHI::Format::RGB32Float, 0, 0 },
                { "COLOR", 0, RHI::Format::RGBA8Unorm, 12, 0 }
            };
            layout.strides[0] = sizeof(LineVertex);
            return layout;
        }

        std::vector<LineVertex> BuildGrid(const RenderView& view)
        {
            std::vector<LineVertex> grid;
            if (!view.showGrid)
            {
                return grid;
            }
            Math::Vector3 center(std::round(view.position.x), 0.0f, std::round(view.position.z));
            uint32_t minorColor = view.gridColor.ToRgba8();
            uint32_t majorColor = view.gridMajorColor.ToRgba8();
            uint32_t axisXColor = view.gridAxisXColor.ToRgba8();
            uint32_t axisZColor = view.gridAxisZColor.ToRgba8();
            for (int i = -GridExtent; i <= GridExtent; ++i)
            {
                bool major = (static_cast<int>(center.x) + i) % GridMajorInterval == 0;
                uint32_t color = major ? majorColor : minorColor;
                float x = center.x + i * GridSpacing;
                grid.push_back({ { x, GridHeight, center.z - GridExtent * GridSpacing }, (static_cast<int>(x) == 0) ? axisZColor : color });
                grid.push_back({ { x, GridHeight, center.z + GridExtent * GridSpacing }, (static_cast<int>(x) == 0) ? axisZColor : color });
                bool majorZ = (static_cast<int>(center.z) + i) % GridMajorInterval == 0;
                uint32_t colorZ = majorZ ? majorColor : minorColor;
                float z = center.z + i * GridSpacing;
                grid.push_back({ { center.x - GridExtent * GridSpacing, GridHeight, z }, (static_cast<int>(z) == 0) ? axisXColor : colorZ });
                grid.push_back({ { center.x + GridExtent * GridSpacing, GridHeight, z }, (static_cast<int>(z) == 0) ? axisXColor : colorZ });
            }
            return grid;
        }
    }

    RHI::Pipeline* LinePass::GetLinePipeline(RHI::Format format, bool depthTest)
    {
        RHI::PipelineDesc desc;
        desc.vertexShader = m_shaders.Get(ShaderProgram::Lines, RHI::ShaderStage::Vertex);
        desc.pixelShader = m_shaders.Get(ShaderProgram::Lines, RHI::ShaderStage::Pixel);
        desc.vertexLayout = LineLayout();
        desc.topology = RHI::PrimitiveTopology::LineList;
        desc.rasterizer.cullMode = RHI::CullMode::None;
        desc.depthStencil.depthTest = depthTest;
        desc.depthStencil.depthWrite = false;
        desc.colorFormats = { format };
        desc.depthFormat = DepthFormat;
        desc.blend[0] = RHI::BlendState::AlphaBlend();
        return m_shaders.GetPipeline({ ShaderProgram::Lines, format, RHI::CullMode::None, depthTest }, desc);
    }

    void LinePass::DrawLines(RHI::CommandList& commands, const std::vector<LineVertex>& lines, RHI::Format format, bool depthTest, float gridFade)
    {
        if (lines.empty())
        {
            return;
        }
        LineConstants constants{};
        Store(constants.params, gridFade, 0.0f, 0.0f, 0.0f);
        commands.SetPipeline(GetLinePipeline(format, depthTest));
        commands.SetConstants(BRIGHT_SLOT_PASS_CONSTANTS, constants);
        commands.SetVertexData(0, lines.data(), lines.size() * sizeof(LineVertex));
        commands.Draw(static_cast<uint32_t>(lines.size()));
    }

    void LinePass::Execute(FrameContext& frame)
    {
        std::vector<LineVertex> grid = BuildGrid(frame.view);
        const auto& depthTested = DebugDraw::GetDepthTested();
        const auto& overlay = DebugDraw::GetOverlay();
        if (grid.empty() && depthTested.empty() && overlay.empty())
        {
            return;
        }
        RHI::CommandList& commands = frame.commands;
        RHI::Format format = frame.output->GetFormat();
        RHI::RenderPassDesc pass;
        pass.colors.push_back({ frame.output, 0, 0, RHI::LoadAction::Load });
        pass.depth.texture = frame.targets.depth.get();
        pass.depth.load = RHI::LoadAction::Load;
        pass.debugName = "Lines";
        commands.BeginRenderPass(pass);
        commands.SetConstants(BRIGHT_SLOT_VIEW_CONSTANTS, frame.viewConstants);
        DrawLines(commands, grid, format, true, GridFadeDistance);
        DrawLines(commands, depthTested, format, true, NoFade);
        DrawLines(commands, overlay, format, false, NoFade);
        commands.EndRenderPass();
    }
}
