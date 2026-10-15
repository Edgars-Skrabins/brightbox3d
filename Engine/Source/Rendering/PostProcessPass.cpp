#include "Rendering/PostProcessPass.h"

#include "Rendering/FrameContext.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/RenderView.h"
#include "Shaders/ShaderBindings.h"

#include <algorithm>
#include <chrono>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr float TemporalHistoryWeight = 0.9f;
        constexpr double MaxExposureDeltaTime = 0.25;

        bool IsLit(const RenderView& view)
        {
            return view.debugMode == RenderDebugMode::Lit;
        }

        bool UsesPostAntiAliasing(const RenderView& view)
        {
            return view.post.antiAliasing == AntiAliasing::FXAA || view.post.antiAliasing == AntiAliasing::SMAA;
        }

        void StoreSourceSize(PostConstants& constants, const RHI::Texture* source)
        {
            Store(constants.sourceSize, static_cast<float>(source->GetWidth()), static_cast<float>(source->GetHeight()), 1.0f / source->GetWidth(), 1.0f / source->GetHeight());
        }

        double SecondsNow()
        {
            return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    }

    void PostProcessPass::Execute(FrameContext& frame)
    {
        RHI::Texture* hdr = ResolveTemporal(frame);
        RHI::Texture* bloom = RenderBloom(frame, hdr);
        Exposure exposure = RenderExposure(frame, hdr);
        RenderTonemap(frame, hdr, bloom, exposure);
    }

    void PostProcessPass::BindCommon(FrameContext& frame, const PostConstants& constants)
    {
        frame.commands.SetConstants(BRIGHT_SLOT_VIEW_CONSTANTS, frame.viewConstants);
        frame.commands.SetConstants(BRIGHT_SLOT_PASS_CONSTANTS, constants);
        frame.commands.SetSampler(BRIGHT_SLOT_POST_LINEAR_SAMPLER, m_resources.GetLinearClampSampler());
        frame.commands.SetSampler(BRIGHT_SLOT_POST_POINT_SAMPLER, m_resources.GetPointClampSampler());
    }

    RHI::Texture* PostProcessPass::ResolveTemporal(FrameContext& frame)
    {
        const RenderView& view = frame.view;
        RenderTargets& targets = frame.targets;
        RHI::Texture* hdr = targets.sceneColor.get();
        if (view.post.antiAliasing != AntiAliasing::TAA || !IsLit(view))
        {
            targets.historyValid = false;
            return hdr;
        }
        RHI::Texture* history = targets.history[targets.historyIndex].get();
        RHI::Texture* resolved = targets.history[1 - targets.historyIndex].get();
        FullscreenPass(frame.commands, resolved, GetFullscreenPipeline(ShaderProgram::TemporalResolve, SceneColorFormat), "TemporalResolve", RHI::LoadAction::DontCare, [&](RHI::CommandList& commands)
        {
            PostConstants constants{};
            Store(constants.params, TemporalHistoryWeight, targets.historyValid ? 0.0f : 1.0f, 0.0f, 0.0f);
            Store(constants.sourceSize, static_cast<float>(view.width), static_cast<float>(view.height), 1.0f / view.width, 1.0f / view.height);
            BindCommon(frame, constants);
            commands.SetTexture(BRIGHT_SLOT_POST_SOURCE, hdr);
            commands.SetTexture(BRIGHT_SLOT_POST_SECOND, history);
            commands.SetTexture(BRIGHT_SLOT_POST_THIRD, targets.velocity.get());
        });
        targets.historyIndex = 1 - targets.historyIndex;
        targets.historyValid = true;
        return resolved;
    }

    RHI::Texture* PostProcessPass::RenderBloom(FrameContext& frame, RHI::Texture* hdr)
    {
        const PostProcessSettings& post = frame.view.post;
        RenderTargets& targets = frame.targets;
        if (post.bloomIntensity <= 0.0f || targets.bloom.empty() || !IsLit(frame.view))
        {
            return m_resources.GetBlackTexture();
        }
        PostConstants constants{};
        Store(constants.params, post.bloomThreshold, 1.0f, 0.0f, 0.0f);
        StoreSourceSize(constants, hdr);
        FullscreenPass(frame.commands, targets.bloom[0].get(), GetFullscreenPipeline(ShaderProgram::BloomPrefilter, BloomFormat), "BloomPrefilter", RHI::LoadAction::DontCare, [&](RHI::CommandList& commands)
        {
            BindCommon(frame, constants);
            commands.SetTexture(BRIGHT_SLOT_POST_SOURCE, hdr);
        });
        for (size_t level = 1; level < targets.bloom.size(); ++level)
        {
            RHI::Texture* source = targets.bloom[level - 1].get();
            StoreSourceSize(constants, source);
            FullscreenPass(frame.commands, targets.bloom[level].get(), GetFullscreenPipeline(ShaderProgram::BloomDownsample, BloomFormat), "BloomDownsample", RHI::LoadAction::DontCare, [&](RHI::CommandList& commands)
            {
                BindCommon(frame, constants);
                commands.SetTexture(BRIGHT_SLOT_POST_SOURCE, source);
            });
        }
        for (size_t level = targets.bloom.size() - 1; level > 0; --level)
        {
            RHI::Texture* source = targets.bloom[level].get();
            StoreSourceSize(constants, source);
            FullscreenPass(frame.commands, targets.bloom[level - 1].get(), GetFullscreenPipeline(ShaderProgram::BloomUpsample, BloomFormat, RHI::BlendState::Additive()), "BloomUpsample", RHI::LoadAction::Load, [&](RHI::CommandList& commands)
            {
                BindCommon(frame, constants);
                commands.SetTexture(BRIGHT_SLOT_POST_SOURCE, source);
            });
        }
        return targets.bloom[0].get();
    }

    PostProcessPass::Exposure PostProcessPass::RenderExposure(FrameContext& frame, RHI::Texture* hdr)
    {
        const PostProcessSettings& post = frame.view.post;
        RenderTargets& targets = frame.targets;
        if (!post.autoExposure || !IsLit(frame.view))
        {
            targets.exposureValid = false;
            return { m_resources.GetWhiteTexture(), false };
        }
        PostConstants constants{};
        StoreSourceSize(constants, hdr);
        FullscreenPass(frame.commands, targets.luminance.get(), GetFullscreenPipeline(ShaderProgram::LogLuminance, LuminanceFormat), "LogLuminance", RHI::LoadAction::DontCare, [&](RHI::CommandList& commands)
        {
            BindCommon(frame, constants);
            commands.SetTexture(BRIGHT_SLOT_POST_SOURCE, hdr);
        });
        double now = SecondsNow();
        float deltaTime = targets.lastTime > 0.0 ? static_cast<float>(std::min(now - targets.lastTime, MaxExposureDeltaTime)) : 0.0f;
        targets.lastTime = now;
        RHI::Texture* previous = targets.exposure[targets.exposureIndex].get();
        RHI::Texture* current = targets.exposure[1 - targets.exposureIndex].get();
        Store(constants.params, post.exposure, post.autoExposureMin, post.autoExposureMax, post.autoExposureSpeed);
        Store(constants.params2, static_cast<float>(LuminanceSize), targets.exposureValid ? 0.0f : 1.0f, deltaTime, 0.0f);
        FullscreenPass(frame.commands, current, GetFullscreenPipeline(ShaderProgram::ExposureAdapt, LuminanceFormat), "ExposureAdapt", RHI::LoadAction::DontCare, [&](RHI::CommandList& commands)
        {
            BindCommon(frame, constants);
            commands.SetTexture(BRIGHT_SLOT_POST_SOURCE, targets.luminance.get());
            commands.SetTexture(BRIGHT_SLOT_POST_SECOND, previous);
        });
        targets.exposureIndex = 1 - targets.exposureIndex;
        targets.exposureValid = true;
        return { current, true };
    }

    void PostProcessPass::RenderTonemap(FrameContext& frame, RHI::Texture* hdr, RHI::Texture* bloom, const Exposure& exposure)
    {
        RHI::Texture* target = UsesPostAntiAliasing(frame.view) ? frame.targets.ldr.get() : frame.output;
        const PostProcessSettings& post = frame.view.post;
        TonemapConstants tonemap{};
        Store(tonemap.exposure, post.exposure, exposure.automatic ? 1.0f : 0.0f, 0.0f, 0.0f);
        Store(tonemap.bloom, bloom != m_resources.GetBlackTexture() ? post.bloomIntensity : 0.0f, 0.0f, 0.0f, 0.0f);
        Store(tonemap.grading, post.saturation, post.contrast, 0.0f, post.colorGrading ? 1.0f : 0.0f);
        StoreColor(tonemap.tint, post.tint, 1.0f);
        Store(tonemap.vignette, post.vignetteIntensity, post.vignetteRadius, post.vignetteSoftness, 0.0f);
        StoreColor(tonemap.vignetteColor, post.vignetteColor, 1.0f);
        StoreColor(tonemap.lift, post.lift, 0.0f);
        StoreColor(tonemap.gamma, post.gamma, 1.0f);
        StoreColor(tonemap.gain, post.gain, 1.0f);
        FullscreenPass(frame.commands, target, GetFullscreenPipeline(ShaderProgram::Tonemap, target->GetFormat()), "Tonemap", RHI::LoadAction::DontCare, [&](RHI::CommandList& commands)
        {
            commands.SetConstants(BRIGHT_SLOT_VIEW_CONSTANTS, frame.viewConstants);
            commands.SetConstants(BRIGHT_SLOT_PASS_CONSTANTS, tonemap);
            commands.SetTexture(BRIGHT_SLOT_TONEMAP_SCENE, hdr);
            commands.SetTexture(BRIGHT_SLOT_TONEMAP_BLOOM, bloom);
            commands.SetTexture(BRIGHT_SLOT_TONEMAP_EXPOSURE, exposure.texture);
            commands.SetTexture(BRIGHT_SLOT_TONEMAP_COLOR_GRADING, m_resources.GetWhiteTexture());
            commands.SetSampler(BRIGHT_SLOT_TONEMAP_SAMPLER, m_resources.GetLinearClampSampler());
        });
    }
}
