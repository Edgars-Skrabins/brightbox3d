#include "Rendering/ShadowPass.h"

#include "Rendering/FrameContext.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/RenderView.h"
#include "Shaders/ShaderBindings.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr uint32_t MinShadowResolution = 256;
        constexpr uint32_t MaxShadowResolution = 8192;
        constexpr float ShadowSlopeBias = 2.0f;
        constexpr float VerticalLightThreshold = 0.99f;
        constexpr float CascadeRadiusSnap = 16.0f;
        constexpr float CascadeBackDistance = 100.0f;
        constexpr int FrustumCornerCount = 8;
        constexpr RHI::Format ShadowMapFormat = RHI::Format::Depth32Float;

        void ComputeSplits(const RenderView& view, float (&splits)[CascadeCount])
        {
            float nearPlane = view.nearPlane;
            float farPlane = std::min(view.shadows.distance, view.farPlane);
            for (uint32_t i = 0; i < CascadeCount; ++i)
            {
                float fraction = static_cast<float>(i + 1) / static_cast<float>(CascadeCount);
                float logarithmic = nearPlane * std::pow(farPlane / nearPlane, fraction);
                float uniform = nearPlane + (farPlane - nearPlane) * fraction;
                splits[i] = Math::Lerp(uniform, logarithmic, view.shadows.splitLambda);
            }
        }
    }

    void ShadowPass::EnsureShadowMap(uint32_t resolution)
    {
        if (m_shadowMap && m_shadowResolution == resolution)
        {
            return;
        }
        m_device.WaitIdle();
        RHI::TextureDesc desc;
        desc.type = RHI::TextureType::Texture2DArray;
        desc.width = resolution;
        desc.height = resolution;
        desc.layers = CascadeCount;
        desc.format = ShadowMapFormat;
        desc.usage = RHI::TextureUsageDepthStencil | RHI::TextureUsageSampled;
        desc.debugName = "CascadedShadowMap";
        m_shadowMap = m_device.CreateTexture(desc);
        m_shadowResolution = resolution;
    }

    ShadowPass::Cascade ShadowPass::FitCascade(const RenderView& view, const Math::Vector3& lightDirection, float sliceNear, float sliceFar, uint32_t resolution) const
    {
        Math::Matrix4 inverseView = view.view.Inverse();
        float aspect = static_cast<float>(view.width) / static_cast<float>(std::max<uint32_t>(view.height, 1));
        float tanHalf = std::tan(view.verticalFov * Math::DegreesToRadians * 0.5f);
        Math::Vector3 up = std::abs(lightDirection.y) > VerticalLightThreshold ? Math::Vector3::Forward() : Math::Vector3::Up();

        Math::Vector3 corners[FrustumCornerCount];
        Math::Vector3 center;
        for (int i = 0; i < FrustumCornerCount; ++i)
        {
            float z = (i & 4) ? sliceFar : sliceNear;
            float halfHeight = z * tanHalf;
            float halfWidth = halfHeight * aspect;
            Math::Vector3 local((i & 1) ? halfWidth : -halfWidth, (i & 2) ? halfHeight : -halfHeight, z);
            corners[i] = inverseView.TransformPoint(local);
            center += corners[i];
        }
        center /= static_cast<float>(FrustumCornerCount);
        float radius = 0.0f;
        for (const Math::Vector3& corner : corners)
        {
            radius = std::max(radius, (corner - center).Length());
        }
        radius = std::ceil(radius * CascadeRadiusSnap) / CascadeRadiusSnap;
        Math::Matrix4 lightView = Math::Matrix4::LookAtLH(center - lightDirection * (radius + CascadeBackDistance), center, up);
        Math::Matrix4 projection = Math::Matrix4::OrthographicOffCenterLH(-radius, radius, -radius, radius, 0.0f, radius * 2.0f + CascadeBackDistance);
        Math::Matrix4 shadowMatrix = lightView * projection;
        Math::Vector3 origin = shadowMatrix.TransformPoint(Math::Vector3::Zero()) * (static_cast<float>(resolution) * 0.5f);
        Math::Vector3 rounded(std::round(origin.x), std::round(origin.y), 0.0f);
        projection.m[3][0] += (rounded.x - origin.x) * 2.0f / static_cast<float>(resolution);
        projection.m[3][1] += (rounded.y - origin.y) * 2.0f / static_cast<float>(resolution);
        return { lightView * projection, radius };
    }

    void ShadowPass::RenderCascade(FrameContext& frame, uint32_t cascade, const Math::Matrix4& shadowMatrix, RHI::Pipeline* pipeline)
    {
        RHI::CommandList& commands = frame.commands;
        RHI::RenderPassDesc pass;
        pass.depth.texture = m_shadowMap.get();
        pass.depth.layer = cascade;
        pass.depth.load = RHI::LoadAction::Clear;
        pass.debugName = "ShadowCascade" + std::to_string(cascade);
        commands.BeginRenderPass(pass);
        ViewConstants viewConstants{};
        viewConstants.viewProjection = shadowMatrix;
        viewConstants.unjitteredViewProjection = shadowMatrix;
        viewConstants.previousViewProjection = shadowMatrix;
        commands.SetConstants(BRIGHT_SLOT_VIEW_CONSTANTS, viewConstants);
        Math::Frustum frustum = Math::Frustum::FromViewProjection(shadowMatrix);
        std::vector<const DrawItem*> casters;
        for (const DrawItem& item : frame.drawItems)
        {
            if (item.castShadows && frustum.Intersects(item.mesh->bounds.Transformed(item.world)))
            {
                casters.push_back(&item);
            }
        }
        m_statistics.shadowCasters += static_cast<uint32_t>(casters.size());
        DrawItems(commands, casters, pipeline, pipeline);
        commands.EndRenderPass();
    }

    void ShadowPass::Execute(FrameContext& frame)
    {
        const RenderView& view = frame.view;
        FrameLighting& lighting = frame.lighting;
        auto* constants = reinterpret_cast<ShadowConstants*>(lighting.shadowConstants.data());
        std::memset(constants, 0, sizeof(ShadowConstants));
        lighting.shadowMap = nullptr;
        if (!lighting.hasShadowLight)
        {
            return;
        }
        uint32_t resolution = std::clamp<uint32_t>(view.shadows.resolution, MinShadowResolution, MaxShadowResolution);
        EnsureShadowMap(resolution);
        lighting.shadowMap = m_shadowMap.get();

        float splits[CascadeCount];
        ComputeSplits(view, splits);
        RHI::Pipeline* pipeline = GetMeshPipeline(ShaderProgram::ShadowDepth, RHI::CullMode::None, {}, ShadowMapFormat, true, {}, false, 0, ShadowSlopeBias);
        for (uint32_t cascade = 0; cascade < CascadeCount; ++cascade)
        {
            float sliceNear = cascade == 0 ? view.nearPlane : splits[cascade - 1];
            Cascade fitted = FitCascade(view, lighting.shadowDirection, sliceNear, splits[cascade], resolution);
            constants->cascades[cascade] = fitted.shadowMatrix;
            constants->splits[cascade] = splits[cascade];
            constants->texelSizes[cascade] = fitted.radius * 2.0f / static_cast<float>(resolution);
            RenderCascade(frame, cascade, fitted.shadowMatrix, pipeline);
        }
        Store(constants->params, 1.0f, 1.0f / static_cast<float>(resolution), view.shadows.normalBias, view.shadows.bias);
    }
}
