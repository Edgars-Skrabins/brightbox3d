#include "Rendering/Renderer.h"

#include "Level/Level.h"
#include "Rendering/AntiAliasingPass.h"
#include "Rendering/FrameContext.h"
#include "Rendering/GBufferPass.h"
#include "Rendering/LightingPass.h"
#include "Rendering/LinePass.h"
#include "Rendering/LocalShadowPass.h"
#include "Rendering/PostProcessPass.h"
#include "Rendering/SSAOPass.h"
#include "Rendering/ShadowPass.h"
#include "Rendering/TransparentPass.h"

#include <algorithm>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr uint32_t TemporalJitterSamples = 8;
        constexpr uint32_t JitterBaseX = 2;
        constexpr uint32_t JitterBaseY = 3;

        float Halton(uint32_t index, uint32_t base)
        {
            float result = 0.0f;
            float fraction = 1.0f / static_cast<float>(base);
            while (index > 0)
            {
                result += fraction * static_cast<float>(index % base);
                index /= base;
                fraction /= static_cast<float>(base);
            }
            return result;
        }

        Math::Matrix4 JitteredProjection(const RenderView& view, uint32_t frameIndex)
        {
            Math::Matrix4 projection = view.projection;
            if (view.post.antiAliasing == AntiAliasing::TAA && view.debugMode == RenderDebugMode::Lit)
            {
                uint32_t sample = (frameIndex % TemporalJitterSamples) + 1;
                float jitterX = Halton(sample, JitterBaseX) - 0.5f;
                float jitterY = Halton(sample, JitterBaseY) - 0.5f;
                projection.m[2][0] += jitterX * 2.0f / static_cast<float>(view.width);
                projection.m[2][1] -= jitterY * 2.0f / static_cast<float>(view.height);
            }
            return projection;
        }

        void FillViewConstants(FrameContext& frame, const Math::Matrix4& jitteredProjection, const Level::Level* level)
        {
            const RenderView& view = frame.view;
            const RenderTargets& targets = frame.targets;
            ViewConstants& viewConstants = frame.viewConstants;
            viewConstants.view = view.view;
            viewConstants.projection = view.projection;
            viewConstants.viewProjection = view.view * jitteredProjection;
            viewConstants.inverseViewProjection = frame.viewProjection.Inverse();
            viewConstants.unjitteredViewProjection = frame.viewProjection;
            viewConstants.previousViewProjection = targets.hasHistory ? targets.previousViewProjection : frame.viewProjection;
            Store(viewConstants.cameraPosition, view.position.x, view.position.y, view.position.z, level ? level->GetTime() : 0.0f);
            Store(viewConstants.screenSize, static_cast<float>(view.width), static_cast<float>(view.height), 1.0f / static_cast<float>(view.width), 1.0f / static_cast<float>(view.height));
            Store(viewConstants.clipPlanes, view.nearPlane, view.farPlane, 0.0f, 0.0f);
            StoreColor(viewConstants.skyTop, view.environment.skyTop, view.environment.skyIntensity);
            StoreColor(viewConstants.skyHorizon, view.environment.skyHorizon, 1.0f);
            StoreColor(viewConstants.skyGround, view.environment.skyGround, 1.0f);
            Store(viewConstants.viewFlags, static_cast<float>(view.debugMode), 0.0f, 0.0f, 0.0f);
        }

        void SortDrawItems(FrameContext& frame)
        {
            for (const DrawItem& item : frame.drawItems)
            {
                if (!item.IsVisible())
                {
                    continue;
                }
                (item.material.transparent ? frame.transparent : frame.opaque).push_back(&item);
            }
            std::sort(frame.opaque.begin(), frame.opaque.end(), [](const DrawItem* a, const DrawItem* b) { return a->distance < b->distance; });
            std::sort(frame.transparent.begin(), frame.transparent.end(), [](const DrawItem* a, const DrawItem* b) { return a->distance > b->distance; });
        }
    }

    Renderer::Renderer(RHI::Device& device, ShaderLibrary& shaders, GpuResourceCache& resources) : m_device(device), m_collector(resources, m_statistics)
    {
        RenderPassResources passResources{ device, shaders, resources, m_statistics };
        m_passes.push_back(std::make_unique<ShadowPass>(passResources));
        m_passes.push_back(std::make_unique<LocalShadowPass>(passResources));
        m_passes.push_back(std::make_unique<GBufferPass>(passResources));
        m_passes.push_back(std::make_unique<SSAOPass>(passResources));
        m_passes.push_back(std::make_unique<LightingPass>(passResources));
        m_passes.push_back(std::make_unique<TransparentPass>(passResources));
        m_passes.push_back(std::make_unique<PostProcessPass>(passResources));
        m_passes.push_back(std::make_unique<AntiAliasingPass>(passResources));
        m_passes.push_back(std::make_unique<LinePass>(passResources));
    }

    Renderer::~Renderer() = default;

    void Renderer::ReleaseView(uint64_t viewId)
    {
        m_views.erase(viewId);
    }

    RHI::Texture* Renderer::GetViewDepth(uint64_t viewId) const
    {
        auto it = m_views.find(viewId);
        return it == m_views.end() ? nullptr : it->second->depth.get();
    }

    RenderTargets& Renderer::GetTargets(const RenderView& view)
    {
        std::unique_ptr<RenderTargets>& targets = m_views[view.id];
        if (!targets)
        {
            targets = std::make_unique<RenderTargets>();
        }
        targets->Ensure(m_device, view);
        return *targets;
    }

    void Renderer::PrepareFrame(FrameContext& frame, const Level::Level* level)
    {
        const RenderView& view = frame.view;
        RenderTargets& targets = frame.targets;
        frame.viewProjection = view.view * view.projection;
        Math::Frustum frustum = Math::Frustum::FromViewProjection(frame.viewProjection);
        Math::Matrix4 jitteredProjection = JitteredProjection(view, targets.frameIndex);
        ++targets.frameIndex;
        m_collector.CollectDraws(level, view, targets, frustum);
        m_collector.CollectLights(level, view, m_lighting);
        FillViewConstants(frame, jitteredProjection, level);
        SortDrawItems(frame);
    }

    void Renderer::Render(RHI::CommandList& commands, const Level::Level* level, const RenderView& view, RHI::Texture* output)
    {
        if (!output || view.width == 0 || view.height == 0)
        {
            return;
        }
        m_statistics = {};
        RenderTargets& targets = GetTargets(view);
        FrameContext frame(commands, view, targets, m_lighting, m_collector.GetDrawItems(), output);
        PrepareFrame(frame, level);
        for (const auto& pass : m_passes)
        {
            pass->Execute(frame);
        }
        targets.previousViewProjection = frame.viewProjection;
        targets.hasHistory = true;
    }
}
