#include "ShaderBindings.h"
#pragma once

#include "Common.hlsl"

#define MAX_LIGHTS 128
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2
#define MAX_LOCAL_SHADOW_VIEWS 64
#define POISSON_SAMPLE_COUNT 16

struct LightData
{
    float4 PositionRange;
    float4 DirectionType;
    float4 ColorIntensity;
    float4 SpotShadow;
};

cbuffer LightConstants : BRIGHT_B(BRIGHT_SLOT_LIGHT_CONSTANTS)
{
    float4 LightCounts;
    float4 AmbientParams;
    LightData Lights[MAX_LIGHTS];
};

cbuffer ShadowConstants : BRIGHT_B(BRIGHT_SLOT_SHADOW_CONSTANTS)
{
    float4x4 CascadeMatrices[4];
    float4 CascadeSplits;
    float4 CascadeTexelSizes;
    float4 ShadowParams;
};

struct LocalShadowViewData
{
    float4x4 ViewProjection;
    float4 AtlasRect;
    float4 Params;
};

cbuffer LocalShadowConstants : BRIGHT_B(BRIGHT_SLOT_LOCAL_SHADOW_CONSTANTS)
{
    float4 LocalShadowSettings;
    LocalShadowViewData LocalShadowViews[MAX_LOCAL_SHADOW_VIEWS];
};

Texture2DArray ShadowMap : BRIGHT_T(BRIGHT_SLOT_SHADOW_MAP);
SamplerComparisonState ShadowSampler : BRIGHT_S(BRIGHT_SLOT_SHADOW_SAMPLER);
Texture2D LocalShadowAtlas : BRIGHT_T(BRIGHT_SLOT_LOCAL_SHADOW_ATLAS);

static const float2 PoissonDisc[POISSON_SAMPLE_COUNT] =
{
    float2(-0.94201624, -0.39906216),
    float2(0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870),
    float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845),
    float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554),
    float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507),
    float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367),
    float2(0.14383161, -0.14100790)
};

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denominator = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denominator * denominator, 1e-6);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0 - f0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 f0, float roughness)
{
    return f0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), f0) - f0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float SampleCascade(int cascade, float3 worldPosition, float3 normal)
{
    float texel = ShadowParams.y;
    float3 offsetPosition = worldPosition + normal * CascadeTexelSizes[cascade] * ShadowParams.z;
    float4 shadowPosition = mul(float4(offsetPosition, 1.0), CascadeMatrices[cascade]);
    float3 coordinates = shadowPosition.xyz / shadowPosition.w;
    float2 uv = float2(coordinates.x * 0.5 + 0.5, 0.5 - coordinates.y * 0.5);
    if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0 || coordinates.z >= 1.0)
    {
        return 1.0;
    }
    float depth = coordinates.z - ShadowParams.w;
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            sum += ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(uv + float2(x, y) * texel, cascade), depth);
        }
    }
    return sum / 9.0;
}

float DirectionalShadow(float3 worldPosition, float3 normal, float viewDepth)
{
    if (ShadowParams.x < 0.5)
    {
        return 1.0;
    }
    int cascade = 3;
    if (viewDepth < CascadeSplits.x)
    {
        cascade = 0;
    }
    else if (viewDepth < CascadeSplits.y)
    {
        cascade = 1;
    }
    else if (viewDepth < CascadeSplits.z)
    {
        cascade = 2;
    }
    else if (viewDepth > CascadeSplits.w)
    {
        return 1.0;
    }
    float shadow = SampleCascade(cascade, worldPosition, normal);
    float splitEnd = cascade == 0 ? CascadeSplits.x : (cascade == 1 ? CascadeSplits.y : (cascade == 2 ? CascadeSplits.z : CascadeSplits.w));
    float fade = saturate((splitEnd - viewDepth) / (splitEnd * 0.1));
    if (cascade < 3 && fade < 1.0)
    {
        shadow = lerp(SampleCascade(cascade + 1, worldPosition, normal), shadow, fade);
    }
    else if (cascade == 3)
    {
        shadow = lerp(1.0, shadow, fade);
    }
    return shadow;
}

int PointShadowFace(float3 toPixel)
{
    float3 axis = abs(toPixel);
    if (axis.x >= axis.y && axis.x >= axis.z)
    {
        return toPixel.x > 0.0 ? 0 : 1;
    }
    if (axis.y >= axis.z)
    {
        return toPixel.y > 0.0 ? 2 : 3;
    }
    return toPixel.z > 0.0 ? 4 : 5;
}

float SampleLocalShadowView(int index, float3 worldPosition, float3 normal, float distanceToLight)
{
    LocalShadowViewData shadowView = LocalShadowViews[index];
    float3 offsetPosition = worldPosition + normal * shadowView.Params.y * distanceToLight;
    float4 shadowPosition = mul(float4(offsetPosition, 1.0), shadowView.ViewProjection);
    if (shadowPosition.w <= 0.0)
    {
        return 1.0;
    }
    float3 coordinates = shadowPosition.xyz / shadowPosition.w;
    float2 uv = float2(coordinates.x * 0.5 + 0.5, 0.5 - coordinates.y * 0.5);
    if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0 || coordinates.z >= 1.0)
    {
        return 1.0;
    }
    float texel = LocalShadowSettings.z;
    float2 tileMin = shadowView.AtlasRect.xy + texel * 0.5;
    float2 tileMax = shadowView.AtlasRect.xy + shadowView.AtlasRect.zw - texel * 0.5;
    float2 center = shadowView.AtlasRect.xy + uv * shadowView.AtlasRect.zw;
    float depth = coordinates.z - shadowView.Params.x;
    float radius = BRIGHT_LOCAL_SHADOW_FILTER_RADIUS * texel;
    float sum = 0.0;
    for (int i = 0; i < POISSON_SAMPLE_COUNT; ++i)
    {
        sum += LocalShadowAtlas.SampleCmpLevelZero(ShadowSampler, clamp(center + PoissonDisc[i] * radius, tileMin, tileMax), depth);
    }
    return sum / POISSON_SAMPLE_COUNT;
}

float LocalShadow(LightData light, float3 worldPosition, float3 normal)
{
    int index = (int)light.SpotShadow.w;
    if (index < 0 || LocalShadowSettings.w < 0.5)
    {
        return 1.0;
    }
    float3 toPixel = worldPosition - light.PositionRange.xyz;
    if ((int)light.DirectionType.w == LIGHT_POINT)
    {
        index += PointShadowFace(toPixel);
    }
    float shadow = SampleLocalShadowView(index, worldPosition, normal, length(toPixel));
    float cameraDistance = length(worldPosition - CameraPosition.xyz);
    float fade = saturate((LocalShadowSettings.x - cameraDistance) / max(LocalShadowSettings.x - LocalShadowSettings.y, 1e-4));
    return lerp(1.0, shadow, fade);
}

float3 EvaluateLight(LightData light, float3 position, float3 normal, float3 view, float3 baseColor, float metallic, float roughness, float viewDepth)
{
    int type = (int)light.DirectionType.w;
    float3 toLight;
    float attenuation = 1.0;
    if (type == LIGHT_DIRECTIONAL)
    {
        toLight = -light.DirectionType.xyz;
        if (light.SpotShadow.z > 0.5)
        {
            attenuation = DirectionalShadow(position, normal, viewDepth);
        }
    }
    else
    {
        float3 offset = light.PositionRange.xyz - position;
        float distanceSquared = dot(offset, offset);
        float range = light.PositionRange.w;
        if (distanceSquared > range * range)
        {
            return float3(0.0, 0.0, 0.0);
        }
        toLight = offset * rsqrt(max(distanceSquared, 1e-8));
        float ratio = distanceSquared / (range * range);
        float window = saturate(1.0 - ratio * ratio);
        attenuation = window * window / max(distanceSquared, 0.0001);
        if (type == LIGHT_SPOT)
        {
            float cosAngle = dot(-toLight, light.DirectionType.xyz);
            attenuation *= smoothstep(light.SpotShadow.y, light.SpotShadow.x, cosAngle);
        }
        if (attenuation > 0.0)
        {
            attenuation *= LocalShadow(light, position, normal);
        }
    }

    float NdotL = saturate(dot(normal, toLight));
    if (NdotL <= 0.0 || attenuation <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }
    float3 halfVector = normalize(view + toLight);
    float NdotV = max(dot(normal, view), 1e-4);
    float NdotH = saturate(dot(normal, halfVector));
    float HdotV = saturate(dot(halfVector, view));
    float clampedRoughness = max(roughness, 0.04);

    float3 f0 = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);
    float3 fresnel = FresnelSchlick(HdotV, f0);
    float distribution = DistributionGGX(NdotH, clampedRoughness);
    float geometry = GeometrySmith(NdotV, NdotL, clampedRoughness);
    float3 specular = distribution * geometry * fresnel / max(4.0 * NdotV * NdotL, 1e-4);
    float3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * baseColor / PI;
    float3 radiance = light.ColorIntensity.rgb * light.ColorIntensity.w * attenuation;
    return (diffuse + specular) * radiance * NdotL;
}

float3 EvaluateAmbient(float3 normal, float3 view, float3 baseColor, float metallic, float roughness, float occlusion)
{
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);
    float NdotV = saturate(dot(normal, view));
    float3 fresnel = FresnelSchlickRoughness(NdotV, f0, roughness);
    float3 irradiance = SampleSky(normal) * AmbientParams.x;
    float3 reflected = reflect(-view, normal);
    float3 prefiltered = lerp(SampleSky(reflected), irradiance, roughness) * AmbientParams.y;
    float3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * baseColor * irradiance;
    float3 specular = prefiltered * fresnel * (1.0 - roughness * 0.7);
    return (diffuse + specular) * occlusion;
}

float3 EvaluateAllLights(float3 position, float3 normal, float3 view, float3 baseColor, float metallic, float roughness, float occlusion, float viewDepth)
{
    float3 color = EvaluateAmbient(normal, view, baseColor, metallic, roughness, occlusion);
    int count = (int)LightCounts.x;
    for (int i = 0; i < count; ++i)
    {
        color += EvaluateLight(Lights[i], position, normal, view, baseColor, metallic, roughness, viewDepth);
    }
    return color;
}
