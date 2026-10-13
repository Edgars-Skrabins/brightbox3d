cbuffer UIConstants : register(b0)
{
    float4x4 ProjectionMatrix;
};

struct UIInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float2 local : TEXCOORD1;
    float2 halfSize : TEXCOORD2;
    float4 params : TEXCOORD3;
    float4 color : COLOR0;
};

struct UIOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float2 local : TEXCOORD1;
    float2 halfSize : TEXCOORD2;
    float4 params : TEXCOORD3;
    float4 color : COLOR0;
};

Texture2D UITexture : register(t0);
SamplerState UISampler : register(s0);

UIOutput UIVS(UIInput input)
{
    UIOutput output;
    output.position = mul(float4(input.position, 0.0, 1.0), ProjectionMatrix);
    output.uv = input.uv;
    output.local = input.local;
    output.halfSize = input.halfSize;
    output.params = input.params;
    output.color = input.color;
    return output;
}

float RoundedBoxDistance(float2 position, float2 halfSize, float radius)
{
    float2 q = abs(position) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float Median(float3 value)
{
    return max(min(value.r, value.g), min(max(value.r, value.g), value.b));
}

float3 EncodeSrgb(float3 color)
{
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(max(color, 0.0), 1.0 / 2.4) - 0.055;
    return float3(color.r <= 0.0031308 ? low.r : high.r, color.g <= 0.0031308 ? low.g : high.g, color.b <= 0.0031308 ? low.b : high.b);
}

float4 UIPS(UIOutput input) : SV_Target0
{
    int mode = (int)(input.params.x + 0.5);
    float4 color = input.color;
    if (mode == 2)
    {
        float4 sample = UITexture.Sample(UISampler, input.uv);
        float2 screenTextureSize = 1.0 / max(fwidth(input.uv), 1e-6);
        float screenRange = max(0.5 * dot(input.params.yz, screenTextureSize), 1.0);
        float distance = Median(sample.rgb) - 0.5;
        float alpha = saturate(distance * screenRange + 0.5);
        return float4(color.rgb, color.a * alpha);
    }
    float radius = input.params.y;
    float distance = RoundedBoxDistance(input.local, input.halfSize, radius);
    float coverage = saturate(0.5 - distance);
    if (mode == 1)
    {
        float inner = saturate(0.5 - (distance + input.params.z));
        coverage = saturate(coverage - inner);
    }
    if (mode == 3 || mode == 4)
    {
        float4 sample = UITexture.Sample(UISampler, input.uv);
        if (mode == 3)
        {
            sample.rgb = EncodeSrgb(sample.rgb);
        }
        color *= sample;
    }
    return float4(color.rgb, color.a * coverage);
}
