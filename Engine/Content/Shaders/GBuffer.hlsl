#include "Surface.hlsl"

struct GBufferOutput
{
    float4 albedo : SV_Target0;
    float4 normalRoughness : SV_Target1;
    float4 material : SV_Target2;
    float4 emission : SV_Target3;
    float2 velocity : SV_Target4;
    float depth : SV_Target5;
};

GBufferOutput GBufferPS(MeshOutput input, bool frontFace : SV_IsFrontFace)
{
    SurfaceData surface = EvaluateSurface(input, frontFace);
    if (MaterialFlags.z > 0.5 && surface.alpha < SurfaceParams.w)
    {
        discard;
    }
    bool unlit = MaterialFlags.y > 0.5;
    GBufferOutput output;
    output.albedo = float4(surface.baseColor, surface.occlusion);
    output.normalRoughness = float4(surface.normal, surface.roughness);
    output.material = float4(surface.metallic, unlit ? 1.0 : 0.0, ObjectParams.x, 1.0);
    output.emission = float4(unlit ? surface.baseColor + surface.emissive : surface.emissive, 1.0);
    float2 current = input.currentClip.xy / input.currentClip.w;
    float2 previous = input.previousClip.xy / input.previousClip.w;
    output.velocity = (current - previous) * float2(0.5, -0.5);
    output.depth = input.position.z;
    return output;
}

void DepthOnlyPS(MeshOutput input)
{
    if (MaterialFlags.z > 0.5)
    {
        float alpha = BaseColorFactor.a;
        if (TextureFlags.x > 0.5)
        {
            alpha *= BaseColorMap.Sample(MaterialSampler, input.uv).a;
        }
        if (alpha < SurfaceParams.w)
        {
            discard;
        }
    }
}
