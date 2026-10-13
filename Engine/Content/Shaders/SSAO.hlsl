#include "Common.hlsl"

Texture2D DepthTexture : register(t0);
Texture2D NormalTexture : register(t1);
Texture2D AOTexture : register(t2);
SamplerState PointClamp : register(s0);

cbuffer SSAOConstants : register(b1)
{
    float4 SSAOParams;
    float4 SSAOKernel[16];
};

float InterleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

float4 SSAOPS(FullscreenOutput input) : SV_Target0
{
    int3 pixel = int3(input.position.xy, 0);
    float depth = DepthTexture.Load(pixel).r;
    if (depth >= 1.0)
    {
        return float4(1.0, 1.0, 1.0, 1.0);
    }
    float3 position = ReconstructWorldPosition(input.uv, depth);
    float3 normal = normalize(NormalTexture.Load(pixel).xyz);
    float angle = InterleavedGradientNoise(input.position.xy) * 6.2831853;
    float3 random = float3(cos(angle), sin(angle), 0.0);
    float3 helper = abs(normal.y) < 0.99 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(helper, normal));
    float3 bitangent = cross(normal, tangent);
    float3 rotatedTangent = normalize(tangent * random.x + bitangent * random.y);
    float3 rotatedBitangent = cross(normal, rotatedTangent);
    float radius = SSAOParams.x;
    float bias = SSAOParams.z;
    float centerDepth = LinearizeDepth(depth);
    float occlusion = 0.0;
    for (int i = 0; i < 16; ++i)
    {
        float3 offset = SSAOKernel[i].x * rotatedTangent + SSAOKernel[i].y * rotatedBitangent + SSAOKernel[i].z * normal;
        float3 samplePosition = position + offset * radius;
        float4 clip = mul(float4(samplePosition, 1.0), UnjitteredViewProjectionMatrix);
        float3 ndc = clip.xyz / clip.w;
        float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
        if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
        {
            continue;
        }
        float sceneDepth = LinearizeDepth(DepthTexture.SampleLevel(PointClamp, uv, 0).r);
        float sampleDepth = clip.w;
        float range = smoothstep(0.0, 1.0, radius / max(abs(centerDepth - sceneDepth), 1e-4));
        occlusion += (sceneDepth <= sampleDepth - bias ? 1.0 : 0.0) * range;
    }
    float ao = 1.0 - occlusion / 16.0;
    ao = saturate(pow(max(ao, 0.0), SSAOParams.y));
    return float4(ao, ao, ao, 1.0);
}

float4 SSAOBlurPS(FullscreenOutput input) : SV_Target0
{
    int2 pixel = int2(input.position.xy);
    float centerDepth = LinearizeDepth(DepthTexture.Load(int3(pixel, 0)).r);
    float sum = 0.0;
    float weight = 0.0;
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            int3 coordinate = int3(pixel + int2(x, y), 0);
            float sampleDepth = LinearizeDepth(DepthTexture.Load(coordinate).r);
            float w = 1.0 / (1.0 + abs(sampleDepth - centerDepth) * 8.0);
            sum += AOTexture.Load(coordinate).r * w;
            weight += w;
        }
    }
    float ao = sum / max(weight, 1e-4);
    return float4(ao, ao, ao, 1.0);
}
