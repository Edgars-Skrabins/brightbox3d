#include "Surface.hlsl"
#include "Lighting.hlsl"

float4 ForwardPS(MeshOutput input, bool frontFace : SV_IsFrontFace) : SV_Target0
{
    SurfaceData surface = EvaluateSurface(input, frontFace);
    float3 view = normalize(CameraPosition.xyz - input.worldPosition);
    float viewDepth = dot(input.worldPosition - CameraPosition.xyz, float3(ViewMatrix[0][2], ViewMatrix[1][2], ViewMatrix[2][2]));
    float3 color;
    if (MaterialFlags.y > 0.5)
    {
        color = surface.baseColor + surface.emissive;
    }
    else
    {
        color = EvaluateAllLights(input.worldPosition, surface.normal, view, surface.baseColor, surface.metallic, surface.roughness, surface.occlusion, viewDepth) + surface.emissive;
    }
    return float4(color * surface.alpha, surface.alpha);
}
