cbuffer ImGuiConstants : register(b0)
{
    float4x4 ProjectionMatrix;
};

struct ImGuiInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct ImGuiOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D ImGuiTexture : register(t0);
SamplerState ImGuiSampler : register(s0);

ImGuiOutput ImGuiVS(ImGuiInput input)
{
    ImGuiOutput output;
    output.position = mul(float4(input.position, 0.0, 1.0), ProjectionMatrix);
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

float4 ImGuiPS(ImGuiOutput input) : SV_Target0
{
    return input.color * ImGuiTexture.Sample(ImGuiSampler, input.uv);
}
