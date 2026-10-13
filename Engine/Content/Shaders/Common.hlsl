#include "ShaderBindings.h"
#pragma once

cbuffer ViewConstants : BRIGHT_B(BRIGHT_SLOT_VIEW_CONSTANTS)
{
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 ViewProjectionMatrix;
    float4x4 InverseViewProjectionMatrix;
    float4x4 UnjitteredViewProjectionMatrix;
    float4x4 PreviousViewProjectionMatrix;
    float4 CameraPosition;
    float4 ScreenSize;
    float4 ClipPlanes;
    float4 SkyTopColor;
    float4 SkyHorizonColor;
    float4 SkyGroundColor;
    float4 ViewFlags;
};

static const float PI = 3.14159265359;

struct FullscreenOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

FullscreenOutput FullscreenVS(uint vertexId : SV_VertexID)
{
    FullscreenOutput output;
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = uv;
    return output;
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    float2 ndc = float2(uv.x * 2.0 - 1.0, (1.0 - uv.y) * 2.0 - 1.0);
    float4 world = mul(float4(ndc, depth, 1.0), InverseViewProjectionMatrix);
    return world.xyz / world.w;
}

float LinearizeDepth(float depth)
{
    float nearPlane = ClipPlanes.x;
    float farPlane = ClipPlanes.y;
    return nearPlane * farPlane / (farPlane - depth * (farPlane - nearPlane));
}

float3 SampleSky(float3 direction)
{
    float up = direction.y;
    float3 color = up >= 0.0 ? lerp(SkyHorizonColor.rgb, SkyTopColor.rgb, pow(saturate(up), 0.6)) : lerp(SkyHorizonColor.rgb, SkyGroundColor.rgb, pow(saturate(-up), 0.4));
    return color * SkyTopColor.w;
}

float Luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 LinearToSrgb(float3 color)
{
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(max(color, 0.0), 1.0 / 2.4) - 0.055;
    return float3(color.r <= 0.0031308 ? low.r : high.r, color.g <= 0.0031308 ? low.g : high.g, color.b <= 0.0031308 ? low.b : high.b);
}
