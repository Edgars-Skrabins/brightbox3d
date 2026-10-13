#include "ShaderBindings.h"
#include "Common.hlsl"

struct LineInput
{
    float3 position : POSITION;
    float4 color : COLOR0;
};

struct LineOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float3 worldPosition : TEXCOORD0;
};

cbuffer LineConstants : BRIGHT_B(BRIGHT_SLOT_PASS_CONSTANTS)
{
    float4 LineParams;
};

LineOutput LineVS(LineInput input)
{
    LineOutput output;
    output.position = mul(float4(input.position, 1.0), ViewProjectionMatrix);
    output.color = input.color;
    output.worldPosition = input.position;
    return output;
}

float4 LinePS(LineOutput input) : SV_Target0
{
    float4 color = input.color;
    if (LineParams.x > 0.0)
    {
        float distanceToCamera = length(input.worldPosition - CameraPosition.xyz);
        color.a *= saturate(1.0 - distanceToCamera / LineParams.x);
    }
    return color;
}
