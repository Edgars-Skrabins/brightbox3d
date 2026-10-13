#include "Common.hlsl"

Texture2D SourceTexture : register(t0);
Texture2D SecondTexture : register(t1);
Texture2D ThirdTexture : register(t2);
SamplerState LinearClamp : register(s0);
SamplerState PointClamp : register(s1);

cbuffer PostConstants : register(b1)
{
    float4 PostParams;
    float4 PostParams2;
    float4 SourceSize;
};

float3 SampleSource(float2 uv)
{
    return SourceTexture.SampleLevel(LinearClamp, uv, 0).rgb;
}

float3 Downsample13(float2 uv)
{
    float2 texel = SourceSize.zw;
    float3 a = SampleSource(uv + texel * float2(-2.0, -2.0));
    float3 b = SampleSource(uv + texel * float2(0.0, -2.0));
    float3 c = SampleSource(uv + texel * float2(2.0, -2.0));
    float3 d = SampleSource(uv + texel * float2(-2.0, 0.0));
    float3 e = SampleSource(uv);
    float3 f = SampleSource(uv + texel * float2(2.0, 0.0));
    float3 g = SampleSource(uv + texel * float2(-2.0, 2.0));
    float3 h = SampleSource(uv + texel * float2(0.0, 2.0));
    float3 i = SampleSource(uv + texel * float2(2.0, 2.0));
    float3 j = SampleSource(uv + texel * float2(-1.0, -1.0));
    float3 k = SampleSource(uv + texel * float2(1.0, -1.0));
    float3 l = SampleSource(uv + texel * float2(-1.0, 1.0));
    float3 m = SampleSource(uv + texel * float2(1.0, 1.0));
    float3 result = e * 0.125;
    result += (a + c + g + i) * 0.03125;
    result += (b + d + f + h) * 0.0625;
    result += (j + k + l + m) * 0.125;
    return result;
}

float4 BloomPrefilterPS(FullscreenOutput input) : SV_Target0
{
    float3 color = Downsample13(input.uv);
    float threshold = PostParams.x;
    float knee = threshold * 0.5 + 1e-4;
    float brightness = max(color.r, max(color.g, color.b));
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    float contribution = max(soft, brightness - threshold) / max(brightness, 1e-4);
    color = min(color * contribution, float3(64.0, 64.0, 64.0));
    return float4(color, 1.0);
}

float4 BloomDownsamplePS(FullscreenOutput input) : SV_Target0
{
    return float4(Downsample13(input.uv), 1.0);
}

float4 BloomUpsamplePS(FullscreenOutput input) : SV_Target0
{
    float2 texel = SourceSize.zw * PostParams.y;
    float3 result = SampleSource(input.uv + float2(-texel.x, -texel.y));
    result += SampleSource(input.uv + float2(0.0, -texel.y)) * 2.0;
    result += SampleSource(input.uv + float2(texel.x, -texel.y));
    result += SampleSource(input.uv + float2(-texel.x, 0.0)) * 2.0;
    result += SampleSource(input.uv) * 4.0;
    result += SampleSource(input.uv + float2(texel.x, 0.0)) * 2.0;
    result += SampleSource(input.uv + float2(-texel.x, texel.y));
    result += SampleSource(input.uv + float2(0.0, texel.y)) * 2.0;
    result += SampleSource(input.uv + float2(texel.x, texel.y));
    return float4(result / 16.0, 1.0);
}

float4 LogLuminancePS(FullscreenOutput input) : SV_Target0
{
    float3 color = SampleSource(input.uv);
    float luminance = max(Luminance(color), 1e-4);
    return float4(log(luminance), 0.0, 0.0, 1.0);
}

float4 ExposureAdaptPS(FullscreenOutput input) : SV_Target0
{
    float sum = 0.0;
    int size = (int)PostParams2.x;
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            sum += SourceTexture.Load(int3(x, y, 0)).r;
        }
    }
    float average = exp(sum / (size * size));
    float target = clamp(0.18 / max(average, 1e-4) * PostParams.x, PostParams.y, PostParams.z);
    float previous = SecondTexture.Load(int3(0, 0, 0)).r;
    if (PostParams2.y > 0.5 || previous <= 0.0)
    {
        previous = target;
    }
    float adapted = previous + (target - previous) * (1.0 - exp(-PostParams.w * PostParams2.z));
    return float4(adapted, adapted, adapted, 1.0);
}

float3 RgbToYCoCg(float3 color)
{
    return float3(dot(color, float3(0.25, 0.5, 0.25)), dot(color, float3(0.5, 0.0, -0.5)), dot(color, float3(-0.25, 0.5, -0.25)));
}

float3 YCoCgToRgb(float3 color)
{
    return float3(color.x + color.y - color.z, color.x + color.z, color.x - color.y - color.z);
}

float4 TemporalResolvePS(FullscreenOutput input) : SV_Target0
{
    int2 pixel = int2(input.position.xy);
    float3 current = SourceTexture.Load(int3(pixel, 0)).rgb;
    float3 minimum = RgbToYCoCg(current);
    float3 maximum = minimum;
    float2 velocity = ThirdTexture.Load(int3(pixel, 0)).xy;
    float closestDepth = 2.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            int3 coordinate = int3(pixel + int2(x, y), 0);
            float3 neighbor = RgbToYCoCg(SourceTexture.Load(coordinate).rgb);
            minimum = min(minimum, neighbor);
            maximum = max(maximum, neighbor);
        }
    }
    float2 historyUV = input.uv - velocity;
    float blend = PostParams.x;
    if (PostParams.y > 0.5 || historyUV.x < 0.0 || historyUV.y < 0.0 || historyUV.x > 1.0 || historyUV.y > 1.0)
    {
        blend = 0.0;
    }
    float3 history = RgbToYCoCg(SecondTexture.SampleLevel(LinearClamp, historyUV, 0).rgb);
    history = YCoCgToRgb(clamp(history, minimum, maximum));
    float currentWeight = (1.0 - blend) / (1.0 + Luminance(current));
    float historyWeight = blend / (1.0 + Luminance(history));
    float3 result = (current * currentWeight + history * historyWeight) / max(currentWeight + historyWeight, 1e-4);
    return float4(result, 1.0);
}

float4 CopyPS(FullscreenOutput input) : SV_Target0
{
    return SourceTexture.SampleLevel(PointClamp, input.uv, 0);
}

float FxaaLuma(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

float4 FXAAPS(FullscreenOutput input) : SV_Target0
{
    float2 texel = SourceSize.zw;
    float2 uv = input.uv;
    float3 center = SampleSource(uv);
    float lumaCenter = FxaaLuma(center);
    float lumaNorth = FxaaLuma(SampleSource(uv + float2(0.0, -texel.y)));
    float lumaSouth = FxaaLuma(SampleSource(uv + float2(0.0, texel.y)));
    float lumaWest = FxaaLuma(SampleSource(uv + float2(-texel.x, 0.0)));
    float lumaEast = FxaaLuma(SampleSource(uv + float2(texel.x, 0.0)));
    float lumaMin = min(lumaCenter, min(min(lumaNorth, lumaSouth), min(lumaWest, lumaEast)));
    float lumaMax = max(lumaCenter, max(max(lumaNorth, lumaSouth), max(lumaWest, lumaEast)));
    float range = lumaMax - lumaMin;
    if (range < max(0.0312, lumaMax * 0.125))
    {
        return float4(center, 1.0);
    }
    float lumaNorthWest = FxaaLuma(SampleSource(uv + float2(-texel.x, -texel.y)));
    float lumaNorthEast = FxaaLuma(SampleSource(uv + float2(texel.x, -texel.y)));
    float lumaSouthWest = FxaaLuma(SampleSource(uv + float2(-texel.x, texel.y)));
    float lumaSouthEast = FxaaLuma(SampleSource(uv + float2(texel.x, texel.y)));
    float horizontal = abs(-2.0 * lumaWest + lumaNorthWest + lumaSouthWest) + abs(-2.0 * lumaCenter + lumaNorth + lumaSouth) * 2.0 + abs(-2.0 * lumaEast + lumaNorthEast + lumaSouthEast);
    float vertical = abs(-2.0 * lumaNorth + lumaNorthWest + lumaNorthEast) + abs(-2.0 * lumaCenter + lumaWest + lumaEast) * 2.0 + abs(-2.0 * lumaSouth + lumaSouthWest + lumaSouthEast);
    bool isHorizontal = horizontal >= vertical;
    float luma1 = isHorizontal ? lumaNorth : lumaWest;
    float luma2 = isHorizontal ? lumaSouth : lumaEast;
    float gradient1 = abs(luma1 - lumaCenter);
    float gradient2 = abs(luma2 - lumaCenter);
    bool steepest1 = gradient1 >= gradient2;
    float gradientScaled = 0.25 * max(gradient1, gradient2);
    float stepLength = isHorizontal ? texel.y : texel.x;
    float lumaLocalAverage;
    if (steepest1)
    {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
    }
    else
    {
        lumaLocalAverage = 0.5 * (luma2 + lumaCenter);
    }
    float2 currentUV = uv;
    if (isHorizontal)
    {
        currentUV.y += stepLength * 0.5;
    }
    else
    {
        currentUV.x += stepLength * 0.5;
    }
    float2 offset = isHorizontal ? float2(texel.x, 0.0) : float2(0.0, texel.y);
    float2 uv1 = currentUV - offset;
    float2 uv2 = currentUV + offset;
    float lumaEnd1 = FxaaLuma(SampleSource(uv1)) - lumaLocalAverage;
    float lumaEnd2 = FxaaLuma(SampleSource(uv2)) - lumaLocalAverage;
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    float steps[10] = { 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 4.0, 8.0 };
    for (int i = 0; i < 10 && !(reached1 && reached2); ++i)
    {
        if (!reached1)
        {
            uv1 -= offset * steps[i];
            lumaEnd1 = FxaaLuma(SampleSource(uv1)) - lumaLocalAverage;
            reached1 = abs(lumaEnd1) >= gradientScaled;
        }
        if (!reached2)
        {
            uv2 += offset * steps[i];
            lumaEnd2 = FxaaLuma(SampleSource(uv2)) - lumaLocalAverage;
            reached2 = abs(lumaEnd2) >= gradientScaled;
        }
    }
    float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);
    bool direction1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = distance1 + distance2;
    float pixelOffset = -distanceFinal / edgeThickness + 0.5;
    bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;
    bool correctVariation = ((direction1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0;
    float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaNorth + lumaSouth + lumaWest + lumaEast) + lumaNorthWest + lumaNorthEast + lumaSouthWest + lumaSouthEast);
    float subPixelOffset1 = saturate(abs(lumaAverage - lumaCenter) / range);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * 0.75;
    finalOffset = max(finalOffset, subPixelOffsetFinal);
    float2 finalUV = uv;
    if (isHorizontal)
    {
        finalUV.y += finalOffset * stepLength;
    }
    else
    {
        finalUV.x += finalOffset * stepLength;
    }
    return float4(SampleSource(finalUV), 1.0);
}
