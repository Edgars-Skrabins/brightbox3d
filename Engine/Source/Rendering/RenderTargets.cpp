#include "Rendering/RenderTargets.h"

#include "Rendering/RenderView.h"

#include <algorithm>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr uint32_t RenderableUsage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
        constexpr uint32_t MaxBloomLevels = 6;
        constexpr uint32_t MinBloomSize = 8;

        std::unique_ptr<RHI::Texture> CreateTarget(RHI::Device& device, uint32_t width, uint32_t height, RHI::Format format, uint32_t usage, const char* name)
        {
            RHI::TextureDesc desc;
            desc.width = width;
            desc.height = height;
            desc.format = format;
            desc.usage = usage;
            desc.debugName = name;
            return device.CreateTexture(desc);
        }

        std::unique_ptr<RHI::Texture> CreatePostTarget(RHI::Device& device, uint32_t width, uint32_t height, RHI::Format format, const char* name)
        {
            return CreateTarget(device, std::max<uint32_t>(width, 1), std::max<uint32_t>(height, 1), format, RenderableUsage, name);
        }
    }

    void RenderTargets::Ensure(RHI::Device& device, const RenderView& view)
    {
        EnsureSceneTargets(device, view);
        EnsurePostTargets(device, view);
    }

    void RenderTargets::EnsureSceneTargets(RHI::Device& device, const RenderView& view)
    {
        if (width == view.width && height == view.height)
        {
            return;
        }
        device.WaitIdle();
        width = view.width;
        height = view.height;
        albedo = CreateTarget(device, width, height, AlbedoFormat, RenderableUsage, "GBufferAlbedo");
        normal = CreateTarget(device, width, height, NormalFormat, RenderableUsage, "GBufferNormal");
        material = CreateTarget(device, width, height, MaterialFormat, RenderableUsage, "GBufferMaterial");
        emission = CreateTarget(device, width, height, EmissionFormat, RenderableUsage, "GBufferEmission");
        velocity = CreateTarget(device, width, height, VelocityFormat, RenderableUsage, "Velocity");
        depthCopy = CreateTarget(device, width, height, DepthCopyFormat, RenderableUsage, "SceneDepthCopy");
        depth = CreateTarget(device, width, height, DepthFormat, RHI::TextureUsageDepthStencil | RHI::TextureUsageSampled, "SceneDepth");
        sceneColor = CreateTarget(device, width, height, SceneColorFormat, RenderableUsage, "SceneColor");
        hasHistory = false;
    }

    void RenderTargets::EnsurePostTargets(RHI::Device& device, const RenderView& view)
    {
        if (ao && ao->GetWidth() == view.width && ao->GetHeight() == view.height)
        {
            return;
        }
        device.WaitIdle();
        ao = CreatePostTarget(device, view.width, view.height, AmbientOcclusionFormat, "SSAO");
        aoBlur = CreatePostTarget(device, view.width, view.height, AmbientOcclusionFormat, "SSAOBlur");
        bloom.clear();
        uint32_t bloomWidth = view.width / 2;
        uint32_t bloomHeight = view.height / 2;
        for (uint32_t level = 0; level < MaxBloomLevels && bloomWidth >= MinBloomSize && bloomHeight >= MinBloomSize; ++level)
        {
            bloom.push_back(CreatePostTarget(device, bloomWidth, bloomHeight, BloomFormat, "Bloom"));
            bloomWidth /= 2;
            bloomHeight /= 2;
        }
        luminance = CreatePostTarget(device, LuminanceSize, LuminanceSize, LuminanceFormat, "Luminance");
        exposure[0] = CreatePostTarget(device, 1, 1, LuminanceFormat, "Exposure0");
        exposure[1] = CreatePostTarget(device, 1, 1, LuminanceFormat, "Exposure1");
        history[0] = CreatePostTarget(device, view.width, view.height, SceneColorFormat, "TemporalHistory0");
        history[1] = CreatePostTarget(device, view.width, view.height, SceneColorFormat, "TemporalHistory1");
        ldr = CreatePostTarget(device, view.width, view.height, LdrFormat, "LDR");
        smaaEdges = CreatePostTarget(device, view.width, view.height, SMAAEdgesFormat, "SMAAEdges");
        smaaBlend = CreatePostTarget(device, view.width, view.height, SMAABlendFormat, "SMAABlend");
        historyValid = false;
        exposureValid = false;
    }
}
