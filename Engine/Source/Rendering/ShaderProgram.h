#pragma once

#include "RHI/Types.h"

#include <cstddef>

namespace BrightBox3D::Rendering
{
    enum class ShaderProgram
    {
        ShadowDepth,
        GBuffer,
        Forward,
        DeferredLighting,
        SSAO,
        SSAOBlur,
        TemporalResolve,
        BloomPrefilter,
        BloomDownsample,
        BloomUpsample,
        LogLuminance,
        ExposureAdapt,
        Tonemap,
        FXAA,
        SMAAEdge,
        SMAABlend,
        SMAANeighborhood,
        Lines,
        UI,
        ImGui,
        Count
    };

    struct ShaderProgramInfo
    {
        const char* name;
        const char* file;
        const char* vertexEntry;
        const char* pixelEntry;
    };

    const ShaderProgramInfo& GetShaderProgramInfo(ShaderProgram program);

    struct PipelineKey
    {
        ShaderProgram program = ShaderProgram::Count;
        RHI::Format format = RHI::Format::Unknown;
        RHI::CullMode cull = RHI::CullMode::None;
        bool depthTest = false;

        bool operator==(const PipelineKey& other) const = default;
    };

    struct PipelineKeyHash
    {
        size_t operator()(const PipelineKey& key) const;
    };
}
