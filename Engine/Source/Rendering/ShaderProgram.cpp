#include "Rendering/ShaderProgram.h"

#include <iterator>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr const char* MeshVertex = "MeshVS";
        constexpr const char* FullscreenVertex = "FullscreenVS";

        constexpr ShaderProgramInfo Programs[] = {
            { "ShadowDepth", "GBuffer.hlsl", MeshVertex, "DepthOnlyPS" },
            { "GBuffer", "GBuffer.hlsl", MeshVertex, "GBufferPS" },
            { "Forward", "Forward.hlsl", MeshVertex, "ForwardPS" },
            { "DeferredLighting", "DeferredLighting.hlsl", FullscreenVertex, "DeferredLightingPS" },
            { "SSAO", "SSAO.hlsl", FullscreenVertex, "SSAOPS" },
            { "SSAOBlur", "SSAO.hlsl", FullscreenVertex, "SSAOBlurPS" },
            { "TemporalResolve", "PostProcess.hlsl", FullscreenVertex, "TemporalResolvePS" },
            { "BloomPrefilter", "PostProcess.hlsl", FullscreenVertex, "BloomPrefilterPS" },
            { "BloomDownsample", "PostProcess.hlsl", FullscreenVertex, "BloomDownsamplePS" },
            { "BloomUpsample", "PostProcess.hlsl", FullscreenVertex, "BloomUpsamplePS" },
            { "LogLuminance", "PostProcess.hlsl", FullscreenVertex, "LogLuminancePS" },
            { "ExposureAdapt", "PostProcess.hlsl", FullscreenVertex, "ExposureAdaptPS" },
            { "Tonemap", "Tonemap.hlsl", FullscreenVertex, "TonemapPS" },
            { "FXAA", "PostProcess.hlsl", FullscreenVertex, "FXAAPS" },
            { "SMAAEdge", "AntiAliasingSMAA.hlsl", "SMAAEdgeVS", "SMAAEdgePS" },
            { "SMAABlend", "AntiAliasingSMAA.hlsl", "SMAABlendVS", "SMAABlendPS" },
            { "SMAANeighborhood", "AntiAliasingSMAA.hlsl", "SMAANeighborhoodVS", "SMAANeighborhoodPS" },
            { "Lines", "Lines.hlsl", "LineVS", "LinePS" },
            { "UI", "UI.hlsl", "UIVS", "UIPS" },
            { "ImGui", "ImGui.hlsl", "ImGuiVS", "ImGuiPS" },
        };

        static_assert(std::size(Programs) == static_cast<size_t>(ShaderProgram::Count));
    }

    const ShaderProgramInfo& GetShaderProgramInfo(ShaderProgram program)
    {
        return Programs[static_cast<size_t>(program)];
    }

    size_t PipelineKeyHash::operator()(const PipelineKey& key) const
    {
        size_t hash = static_cast<size_t>(key.program);
        hash = hash * 31 + static_cast<size_t>(key.format);
        hash = hash * 31 + static_cast<size_t>(key.cull);
        hash = hash * 31 + static_cast<size_t>(key.depthTest);
        return hash;
    }
}
