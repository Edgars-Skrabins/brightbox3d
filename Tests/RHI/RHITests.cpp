#include "Math/Matrix4.h"
#include "RHI/Device.h"

#include <gtest/gtest.h>

using namespace BrightBox3D;
using namespace BrightBox3D::RHI;

namespace
{
    const char* TriangleShader = R"(
cbuffer Params : register(b0)
{
    float4x4 Transform;
    float4 Tint;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D Albedo : register(t1);
SamplerState AlbedoSampler : register(s2);

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), Transform);
    output.color = input.color * Tint;
    output.uv = input.uv;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    return input.color;
}

float4 PSTextured(VSOutput input) : SV_Target0
{
    return Albedo.Sample(AlbedoSampler, input.uv) * input.color;
}
)";

    struct Vertex
    {
        float position[3];
        float color[4];
        float uv[2];
    };

    struct Params
    {
        Math::Matrix4 transform;
        float tint[4];
    };

    VertexLayout MakeLayout()
    {
        VertexLayout layout;
        layout.attributes = {
            { "POSITION", 0, Format::RGB32Float, 0, 0 },
            { "COLOR", 0, Format::RGBA32Float, 12, 0 },
            { "TEXCOORD", 0, Format::RG32Float, 28, 0 }
        };
        layout.strides[0] = sizeof(Vertex);
        return layout;
    }

    std::array<uint8_t, 4> Pixel(const std::vector<uint8_t>& pixels, uint32_t width, uint32_t x, uint32_t y)
    {
        size_t index = (static_cast<size_t>(y) * width + x) * 4;
        return { pixels[index], pixels[index + 1], pixels[index + 2], pixels[index + 3] };
    }
}

TEST(RHITests, RendersTriangleWithConstantsAndReadsBack)
{
    std::unique_ptr<Device> device = Device::Create();
    ASSERT_NE(device, nullptr);

    std::string error;
    ShaderDesc vertexDesc{ TriangleShader, "VSMain", ShaderStage::Vertex, {}, "TriangleVS" };
    ShaderDesc pixelDesc{ TriangleShader, "PSMain", ShaderStage::Pixel, {}, "TrianglePS" };
    auto vertexShader = device->CreateShader(vertexDesc, error);
    ASSERT_NE(vertexShader, nullptr) << error;
    auto pixelShader = device->CreateShader(pixelDesc, error);
    ASSERT_NE(pixelShader, nullptr) << error;

    TextureDesc targetDesc;
    targetDesc.width = 64;
    targetDesc.height = 64;
    targetDesc.format = Format::RGBA8Unorm;
    targetDesc.usage = TextureUsageRenderTarget | TextureUsageSampled;
    auto target = device->CreateTexture(targetDesc);
    TextureDesc depthDesc = targetDesc;
    depthDesc.format = Format::Depth32Float;
    depthDesc.usage = TextureUsageDepthStencil;
    auto depth = device->CreateTexture(depthDesc);

    PipelineDesc pipelineDesc;
    pipelineDesc.vertexShader = vertexShader.get();
    pipelineDesc.pixelShader = pixelShader.get();
    pipelineDesc.vertexLayout = MakeLayout();
    pipelineDesc.rasterizer.cullMode = CullMode::Back;
    pipelineDesc.colorFormats = { Format::RGBA8Unorm };
    pipelineDesc.depthFormat = Format::Depth32Float;
    auto pipeline = device->CreatePipeline(pipelineDesc, error);
    ASSERT_NE(pipeline, nullptr) << error;

    Vertex vertices[] = {
        { { -0.5f, -0.5f, 0.5f }, { 1, 1, 1, 1 }, { 0, 1 } },
        { { 0.0f, 0.5f, 0.5f }, { 1, 1, 1, 1 }, { 0.5f, 0 } },
        { { 0.5f, -0.5f, 0.5f }, { 1, 1, 1, 1 }, { 1, 1 } }
    };
    BufferDesc bufferDesc{ sizeof(vertices), BufferUsage::Vertex, false, "Triangle" };
    auto vertexBuffer = device->CreateBuffer(bufferDesc, vertices);

    Params params;
    params.transform = Math::Matrix4::Translation({ 0.25f, 0.0f, 0.0f });
    params.tint[0] = 1.0f;
    params.tint[1] = 0.0f;
    params.tint[2] = 0.0f;
    params.tint[3] = 1.0f;

    CommandList& commands = device->BeginFrame();
    RenderPassDesc pass;
    pass.colors.push_back({ target.get(), 0, 0, LoadAction::Clear, { 0.0f, 0.0f, 1.0f, 1.0f } });
    pass.depth.texture = depth.get();
    commands.BeginRenderPass(pass);
    commands.SetPipeline(pipeline.get());
    commands.SetVertexBuffer(0, vertexBuffer.get());
    commands.SetConstants(0, params);
    commands.Draw(3);
    commands.EndRenderPass();
    device->EndFrame({});

    std::vector<uint8_t> pixels;
    ASSERT_TRUE(device->ReadTexture(target.get(), pixels));
    auto center = Pixel(pixels, 64, 40, 34);
    EXPECT_EQ(center[0], 255);
    EXPECT_EQ(center[1], 0);
    EXPECT_EQ(center[2], 0);
    auto corner = Pixel(pixels, 64, 2, 2);
    EXPECT_EQ(corner[0], 0);
    EXPECT_EQ(corner[2], 255);
    auto leftOfTriangle = Pixel(pixels, 64, 14, 40);
    EXPECT_EQ(leftOfTriangle[2], 255);
}

TEST(RHITests, SamplesTexturesWithRemappedBindings)
{
    std::unique_ptr<Device> device = Device::Create();
    ASSERT_NE(device, nullptr);
    std::string error;
    auto vertexShader = device->CreateShader({ TriangleShader, "VSMain", ShaderStage::Vertex, {}, "VS" }, error);
    ASSERT_NE(vertexShader, nullptr) << error;
    auto pixelShader = device->CreateShader({ TriangleShader, "PSTextured", ShaderStage::Pixel, {}, "PS" }, error);
    ASSERT_NE(pixelShader, nullptr) << error;

    TextureDesc targetDesc;
    targetDesc.width = 32;
    targetDesc.height = 32;
    targetDesc.format = Format::RGBA8Unorm;
    targetDesc.usage = TextureUsageRenderTarget;
    auto target = device->CreateTexture(targetDesc);

    TextureDesc albedoDesc;
    albedoDesc.width = 2;
    albedoDesc.height = 1;
    albedoDesc.format = Format::RGBA8Unorm;
    auto albedo = device->CreateTexture(albedoDesc);
    uint8_t texels[] = { 0, 255, 0, 255, 255, 255, 0, 255 };
    device->UpdateTexture(albedo.get(), 0, 0, texels, 8);
    SamplerDesc samplerDesc;
    samplerDesc.filter = Filter::Point;
    samplerDesc.addressU = AddressMode::Clamp;
    samplerDesc.addressV = AddressMode::Clamp;
    auto sampler = device->CreateSampler(samplerDesc);

    PipelineDesc pipelineDesc;
    pipelineDesc.vertexShader = vertexShader.get();
    pipelineDesc.pixelShader = pixelShader.get();
    pipelineDesc.vertexLayout = MakeLayout();
    pipelineDesc.rasterizer.cullMode = CullMode::None;
    pipelineDesc.depthStencil.depthTest = false;
    pipelineDesc.colorFormats = { Format::RGBA8Unorm };
    auto pipeline = device->CreatePipeline(pipelineDesc, error);
    ASSERT_NE(pipeline, nullptr) << error;

    Vertex quad[] = {
        { { -1, -1, 0 }, { 1, 1, 1, 1 }, { 0, 1 } },
        { { -1, 1, 0 }, { 1, 1, 1, 1 }, { 0, 0 } },
        { { 1, -1, 0 }, { 1, 1, 1, 1 }, { 1, 1 } },
        { { 1, 1, 0 }, { 1, 1, 1, 1 }, { 1, 0 } }
    };
    uint16_t indices[] = { 0, 1, 2, 2, 1, 3 };
    Params params;
    params.tint[0] = params.tint[1] = params.tint[2] = params.tint[3] = 1.0f;

    CommandList& commands = device->BeginFrame();
    RenderPassDesc pass;
    pass.colors.push_back({ target.get(), 0, 0, LoadAction::Clear, { 0, 0, 0, 1 } });
    commands.BeginRenderPass(pass);
    commands.SetPipeline(pipeline.get());
    commands.SetVertexData(0, quad, sizeof(quad));
    commands.SetIndexData(indices, sizeof(indices), IndexFormat::UInt16);
    commands.SetConstants(0, params);
    commands.SetTexture(1, albedo.get());
    commands.SetSampler(2, sampler.get());
    commands.DrawIndexed(6);
    commands.EndRenderPass();
    device->EndFrame({});

    std::vector<uint8_t> pixels;
    ASSERT_TRUE(device->ReadTexture(target.get(), pixels));
    auto left = Pixel(pixels, 32, 4, 16);
    auto right = Pixel(pixels, 32, 28, 16);
    EXPECT_EQ(left[0], 0);
    EXPECT_EQ(left[1], 255);
    EXPECT_EQ(right[0], 255);
    EXPECT_EQ(right[1], 255);
    EXPECT_EQ(right[2], 0);
}
