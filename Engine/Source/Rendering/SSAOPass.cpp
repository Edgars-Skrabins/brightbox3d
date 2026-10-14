#include "Rendering/SSAOPass.h"

#include "Math/MathUtils.h"
#include "Math/Vector3.h"
#include "Rendering/FrameContext.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/RenderView.h"
#include "Shaders/ShaderBindings.h"

#include <cstring>
#include <random>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr uint32_t KernelSeed = 1337;
        constexpr float MinimumSampleHeight = 0.1f;
        constexpr float SampleHeightRange = 0.9f;
        constexpr float MinimumSampleScale = 0.1f;
        constexpr float MaximumSampleScale = 1.0f;
        constexpr float DepthBias = 0.02f;

        bool UsesAmbientOcclusion(const RenderView& view)
        {
            return view.post.ssao && (view.debugMode == RenderDebugMode::Lit || view.debugMode == RenderDebugMode::AmbientOcclusion);
        }
    }

    SSAOPass::SSAOPass(const RenderPassResources& resources) : RenderPass(resources)
    {
        std::mt19937 random(KernelSeed);
        std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
        for (uint32_t i = 0; i < SSAOKernelSize; ++i)
        {
            Math::Vector3 sample(distribution(random) * 2.0f - 1.0f, distribution(random) * 2.0f - 1.0f, distribution(random) * SampleHeightRange + MinimumSampleHeight);
            sample = sample.Normalized() * distribution(random);
            float scale = static_cast<float>(i) / static_cast<float>(SSAOKernelSize);
            scale = Math::Lerp(MinimumSampleScale, MaximumSampleScale, scale * scale);
            sample *= scale;
            Store(m_kernel[i], sample.x, sample.y, sample.z, 0.0f);
        }
    }

    void SSAOPass::Execute(FrameContext& frame)
    {
        frame.ambientOcclusion = m_resources.GetWhiteTexture();
        if (!UsesAmbientOcclusion(frame.view))
        {
            return;
        }
        RHI::CommandList& commands = frame.commands;
        RenderTargets& targets = frame.targets;
        const ViewConstants& viewConstants = frame.viewConstants;
        SSAOConstants ssao{};
        Store(ssao.params, frame.view.post.ssaoRadius, frame.view.post.ssaoIntensity, DepthBias, static_cast<float>(SSAOKernelSize));
        std::memcpy(ssao.kernel, m_kernel, sizeof(m_kernel));
        FullscreenPass(commands, targets.ao.get(), GetFullscreenPipeline(ShaderProgram::SSAO, AmbientOcclusionFormat), "SSAO", false, [&](RHI::CommandList&)
        {
            commands.SetConstants(BRIGHT_SLOT_VIEW_CONSTANTS, viewConstants);
            commands.SetConstants(BRIGHT_SLOT_PASS_CONSTANTS, ssao);
            commands.SetTexture(BRIGHT_SLOT_SSAO_DEPTH, targets.depthCopy.get());
            commands.SetTexture(BRIGHT_SLOT_SSAO_NORMAL, targets.normal.get());
            commands.SetSampler(BRIGHT_SLOT_SSAO_SAMPLER, m_resources.GetPointClampSampler());
        });
        FullscreenPass(commands, targets.aoBlur.get(), GetFullscreenPipeline(ShaderProgram::SSAOBlur, AmbientOcclusionFormat), "SSAOBlur", false, [&](RHI::CommandList&)
        {
            commands.SetConstants(BRIGHT_SLOT_VIEW_CONSTANTS, viewConstants);
            commands.SetConstants(BRIGHT_SLOT_PASS_CONSTANTS, ssao);
            commands.SetTexture(BRIGHT_SLOT_SSAO_DEPTH, targets.depthCopy.get());
            commands.SetTexture(BRIGHT_SLOT_SSAO_OCCLUSION, targets.ao.get());
        });
        frame.ambientOcclusion = targets.aoBlur.get();
    }
}
