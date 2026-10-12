#include "Lighting.hlsl"

Texture2D AlbedoTexture : register(t0);
Texture2D NormalRoughnessTexture : register(t1);
Texture2D MaterialTexture : register(t2);
Texture2D EmissionTexture : register(t3);
Texture2D DepthTexture : register(t4);
Texture2D AmbientOcclusionTexture : register(t5);

float4 DeferredLightingPS(FullscreenOutput input) : SV_Target0
{
    int3 pixel = int3(input.position.xy, 0);
    float depth = DepthTexture.Load(pixel).r;
    float3 worldPosition = ReconstructWorldPosition(input.uv, depth);
    float3 viewDirection = normalize(worldPosition - CameraPosition.xyz);
    if (depth >= 1.0)
    {
        return float4(SampleSky(viewDirection), 1.0);
    }
    float4 albedo = AlbedoTexture.Load(pixel);
    float4 normalRoughness = NormalRoughnessTexture.Load(pixel);
    float4 material = MaterialTexture.Load(pixel);
    float3 emission = EmissionTexture.Load(pixel).rgb;
    if (material.y > 0.5)
    {
        return float4(emission, 1.0);
    }
    float occlusion = albedo.a * AmbientOcclusionTexture.Load(pixel).r;
    float3 normal = normalize(normalRoughness.xyz);
    float viewDepth = LinearizeDepth(depth);
    int debugMode = (int)ViewFlags.x;
    if (debugMode == 1)
    {
        return float4(albedo.rgb, 1.0);
    }
    if (debugMode == 2)
    {
        return float4(normal * 0.5 + 0.5, 1.0);
    }
    if (debugMode == 3)
    {
        return float4(material.x, normalRoughness.w, 0.0, 1.0);
    }
    if (debugMode == 4)
    {
        float shadow = DirectionalShadow(worldPosition, normal, viewDepth);
        return float4(shadow, shadow, shadow, 1.0);
    }
    if (debugMode == 5)
    {
        return float4(occlusion, occlusion, occlusion, 1.0);
    }
    float3 color = EvaluateAllLights(worldPosition, normal, -viewDirection, albedo.rgb, material.x, normalRoughness.w, occlusion, viewDepth);
    return float4(color + emission, 1.0);
}
