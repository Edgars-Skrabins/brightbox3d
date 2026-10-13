#include "ShaderBindings.h"
#define SMAA_CUSTOM_SL 1
#define SMAA_PRESET_HIGH 1

cbuffer SMAAConstants : BRIGHT_B(BRIGHT_SLOT_LOCAL_CONSTANTS)
{
    float4 SMAAMetrics;
};

#define SMAA_RT_METRICS SMAAMetrics

SamplerState LinearSampler : BRIGHT_S(BRIGHT_SLOT_SMAA_LINEAR_SAMPLER);
SamplerState PointSampler : BRIGHT_S(BRIGHT_SLOT_SMAA_POINT_SAMPLER);

#define SMAATexture2D(tex) Texture2D tex
#define SMAATexturePass2D(tex) tex
#define SMAASampleLevelZero(tex, coord) tex.SampleLevel(LinearSampler, coord, 0)
#define SMAASampleLevelZeroPoint(tex, coord) tex.SampleLevel(PointSampler, coord, 0)
#define SMAASampleLevelZeroOffset(tex, coord, offset) tex.SampleLevel(LinearSampler, coord, 0, offset)
#define SMAASample(tex, coord) tex.Sample(LinearSampler, coord)
#define SMAASamplePoint(tex, coord) tex.Sample(PointSampler, coord)
#define SMAASampleOffset(tex, coord, offset) tex.Sample(LinearSampler, coord, offset)
#define SMAA_FLATTEN [flatten]
#define SMAA_BRANCH [branch]

#include "ThirdParty/SMAA/SMAA.hlsl"

Texture2D ColorTexture : BRIGHT_T(BRIGHT_SLOT_SMAA_COLOR);
Texture2D EdgesTexture : BRIGHT_T(BRIGHT_SLOT_SMAA_EDGES);
Texture2D AreaTexture : BRIGHT_T(BRIGHT_SLOT_SMAA_AREA);
Texture2D SearchTexture : BRIGHT_T(BRIGHT_SLOT_SMAA_SEARCH);
Texture2D BlendTexture : BRIGHT_T(BRIGHT_SLOT_SMAA_BLEND);

struct SMAAOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 offset0 : TEXCOORD1;
    float4 offset1 : TEXCOORD2;
    float4 offset2 : TEXCOORD3;
    float2 pixcoord : TEXCOORD4;
};

float2 FullscreenUV(uint vertexId)
{
    return float2((vertexId << 1) & 2, vertexId & 2);
}

float4 FullscreenPosition(float2 uv)
{
    return float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

SMAAOutput SMAAEdgeVS(uint vertexId : SV_VertexID)
{
    SMAAOutput output;
    output.uv = FullscreenUV(vertexId);
    output.position = FullscreenPosition(output.uv);
    float4 offset[3];
    SMAAEdgeDetectionVS(output.uv, offset);
    output.offset0 = offset[0];
    output.offset1 = offset[1];
    output.offset2 = offset[2];
    output.pixcoord = float2(0.0, 0.0);
    return output;
}

float4 SMAAEdgePS(SMAAOutput input) : SV_Target0
{
    float4 offset[3] = { input.offset0, input.offset1, input.offset2 };
    return float4(SMAALumaEdgeDetectionPS(input.uv, offset, ColorTexture), 0.0, 0.0);
}

SMAAOutput SMAABlendVS(uint vertexId : SV_VertexID)
{
    SMAAOutput output;
    output.uv = FullscreenUV(vertexId);
    output.position = FullscreenPosition(output.uv);
    float4 offset[3];
    float2 pixcoord;
    SMAABlendingWeightCalculationVS(output.uv, pixcoord, offset);
    output.offset0 = offset[0];
    output.offset1 = offset[1];
    output.offset2 = offset[2];
    output.pixcoord = pixcoord;
    return output;
}

float4 SMAABlendPS(SMAAOutput input) : SV_Target0
{
    float4 offset[3] = { input.offset0, input.offset1, input.offset2 };
    return SMAABlendingWeightCalculationPS(input.uv, input.pixcoord, offset, EdgesTexture, AreaTexture, SearchTexture, float4(0.0, 0.0, 0.0, 0.0));
}

SMAAOutput SMAANeighborhoodVS(uint vertexId : SV_VertexID)
{
    SMAAOutput output;
    output.uv = FullscreenUV(vertexId);
    output.position = FullscreenPosition(output.uv);
    float4 offset;
    SMAANeighborhoodBlendingVS(output.uv, offset);
    output.offset0 = offset;
    output.offset1 = float4(0.0, 0.0, 0.0, 0.0);
    output.offset2 = float4(0.0, 0.0, 0.0, 0.0);
    output.pixcoord = float2(0.0, 0.0);
    return output;
}

float4 SMAANeighborhoodPS(SMAAOutput input) : SV_Target0
{
    return SMAANeighborhoodBlendingPS(input.uv, input.offset0, ColorTexture, BlendTexture);
}
