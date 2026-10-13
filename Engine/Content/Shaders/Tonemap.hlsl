#include "ShaderBindings.h"
#include "Common.hlsl"

Texture2D SceneColor : BRIGHT_T(BRIGHT_SLOT_TONEMAP_SCENE);
Texture2D BloomTexture : BRIGHT_T(BRIGHT_SLOT_TONEMAP_BLOOM);
Texture2D ExposureTexture : BRIGHT_T(BRIGHT_SLOT_TONEMAP_EXPOSURE);
Texture2D ColorGradingLut : BRIGHT_T(BRIGHT_SLOT_TONEMAP_COLOR_GRADING);
SamplerState LinearClamp : BRIGHT_S(BRIGHT_SLOT_TONEMAP_SAMPLER);

cbuffer TonemapConstants : BRIGHT_B(BRIGHT_SLOT_PASS_CONSTANTS)
{
    float4 ExposureParams;
    float4 BloomParams;
    float4 GradingParams;
    float4 GradingTint;
    float4 VignetteParams;
    float4 VignetteColor;
    float4 GradingLift;
    float4 GradingGamma;
    float4 GradingGain;
};

float3 AcesFitted(float3 color)
{
    float3x3 inputMatrix = float3x3(0.59719, 0.35458, 0.04823, 0.07600, 0.90834, 0.01566, 0.02840, 0.13383, 0.83777);
    float3x3 outputMatrix = float3x3(1.60475, -0.53108, -0.07367, -0.10208, 1.10813, -0.00605, -0.00327, -0.07276, 1.07602);
    color = mul(inputMatrix, color);
    float3 a = color * (color + 0.0245786) - 0.000090537;
    float3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
    color = a / b;
    return saturate(mul(outputMatrix, color));
}

float3 ApplyGrading(float3 color)
{
    float luminance = Luminance(color);
    color = lerp(float3(luminance, luminance, luminance), color, GradingParams.x);
    color = (color - 0.5) * GradingParams.y + 0.5;
    color *= GradingTint.rgb;
    color = color * GradingGain.rgb + GradingLift.rgb * (1.0 - color);
    color = pow(max(color, 0.0), 1.0 / max(GradingGamma.rgb, 0.01));
    return color;
}

float4 TonemapPS(FullscreenOutput input) : SV_Target0
{
    float3 color = SceneColor.SampleLevel(LinearClamp, input.uv, 0).rgb;
    if (BloomParams.x > 0.0)
    {
        color += BloomTexture.SampleLevel(LinearClamp, input.uv, 0).rgb * BloomParams.x;
    }
    float exposure = ExposureParams.y > 0.5 ? ExposureTexture.Load(int3(0, 0, 0)).r : ExposureParams.x;
    color *= exposure;
    color = AcesFitted(color);
    if (GradingParams.w > 0.5)
    {
        color = saturate(ApplyGrading(color));
    }
    if (VignetteParams.x > 0.0)
    {
        float2 centered = input.uv - 0.5;
        centered.x *= ScreenSize.x * ScreenSize.w;
        float vignette = smoothstep(VignetteParams.y, VignetteParams.y - VignetteParams.z, length(centered));
        color = lerp(VignetteColor.rgb, color, lerp(1.0, vignette, VignetteParams.x));
    }
    if (ViewFlags.x > 0.5)
    {
        return float4(LinearToSrgb(saturate(SceneColor.SampleLevel(LinearClamp, input.uv, 0).rgb)), 1.0);
    }
    return float4(LinearToSrgb(color), 1.0);
}
