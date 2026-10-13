#include "ShaderBindings.h"
#pragma once

#include "Common.hlsl"

cbuffer ObjectConstants : BRIGHT_B(BRIGHT_SLOT_OBJECT_CONSTANTS)
{
    float4x4 WorldMatrix;
    float4x4 PreviousWorldMatrix;
    float4x4 NormalMatrix;
    float4 ObjectParams;
};

cbuffer MaterialConstants : BRIGHT_B(BRIGHT_SLOT_MATERIAL_CONSTANTS)
{
    float4 BaseColorFactor;
    float4 EmissiveFactor;
    float4 SurfaceParams;
    float4 UVTransform;
    float4 TextureFlags;
    float4 MaterialFlags;
};

Texture2D BaseColorMap : BRIGHT_T(BRIGHT_SLOT_BASE_COLOR_MAP);
Texture2D NormalMap : BRIGHT_T(BRIGHT_SLOT_NORMAL_MAP);
Texture2D MetallicRoughnessMap : BRIGHT_T(BRIGHT_SLOT_METALLIC_ROUGHNESS_MAP);
Texture2D OcclusionMap : BRIGHT_T(BRIGHT_SLOT_OCCLUSION_MAP);
Texture2D EmissiveMap : BRIGHT_T(BRIGHT_SLOT_EMISSIVE_MAP);
SamplerState MaterialSampler : BRIGHT_S(BRIGHT_SLOT_MATERIAL_SAMPLER);

struct MeshInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};

struct MeshOutput
{
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 tangent : TEXCOORD2;
    float2 uv : TEXCOORD3;
    float4 currentClip : TEXCOORD4;
    float4 previousClip : TEXCOORD5;
};

MeshOutput MeshVS(MeshInput input)
{
    MeshOutput output;
    float4 world = mul(float4(input.position, 1.0), WorldMatrix);
    output.worldPosition = world.xyz;
    output.position = mul(world, ViewProjectionMatrix);
    output.currentClip = mul(world, UnjitteredViewProjectionMatrix);
    float4 previousWorld = mul(float4(input.position, 1.0), PreviousWorldMatrix);
    output.previousClip = mul(previousWorld, PreviousViewProjectionMatrix);
    output.normal = normalize(mul(float4(input.normal, 0.0), NormalMatrix).xyz);
    output.tangent = float4(normalize(mul(float4(input.tangent.xyz, 0.0), WorldMatrix).xyz), input.tangent.w);
    output.uv = input.uv * UVTransform.xy + UVTransform.zw;
    return output;
}

struct SurfaceData
{
    float3 baseColor;
    float alpha;
    float3 normal;
    float metallic;
    float roughness;
    float occlusion;
    float3 emissive;
};

SurfaceData EvaluateSurface(MeshOutput input, bool frontFace)
{
    SurfaceData surface;
    float4 baseColor = BaseColorFactor;
    if (TextureFlags.x > 0.5)
    {
        baseColor *= BaseColorMap.Sample(MaterialSampler, input.uv);
    }
    surface.baseColor = baseColor.rgb;
    surface.alpha = baseColor.a;

    float3 normal = normalize(input.normal);
    if (!frontFace)
    {
        normal = -normal;
    }
    if (TextureFlags.y > 0.5)
    {
        float3 tangent = normalize(input.tangent.xyz - normal * dot(input.tangent.xyz, normal));
        float3 bitangent = cross(normal, tangent) * input.tangent.w;
        float3 sampled = NormalMap.Sample(MaterialSampler, input.uv).xyz * 2.0 - 1.0;
        sampled.y = -sampled.y;
        sampled.xy *= SurfaceParams.z;
        normal = normalize(sampled.x * tangent + sampled.y * bitangent + sampled.z * normal);
    }
    surface.normal = normal;

    surface.metallic = SurfaceParams.x;
    surface.roughness = SurfaceParams.y;
    if (TextureFlags.z > 0.5)
    {
        float4 metallicRoughness = MetallicRoughnessMap.Sample(MaterialSampler, input.uv);
        surface.roughness *= metallicRoughness.g;
        surface.metallic *= metallicRoughness.b;
    }
    surface.occlusion = TextureFlags.w > 0.5 ? OcclusionMap.Sample(MaterialSampler, input.uv).r : 1.0;
    surface.emissive = EmissiveFactor.rgb;
    if (MaterialFlags.x > 0.5)
    {
        surface.emissive *= EmissiveMap.Sample(MaterialSampler, input.uv).rgb;
    }
    return surface;
}
