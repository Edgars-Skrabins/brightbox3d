#include "Rendering/LocalShadowPass.h"

#include "Math/MathUtils.h"
#include "Rendering/FrameContext.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/RenderView.h"
#include "Shaders/ShaderBindings.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr uint32_t PointLightFaceCount = 6;
        constexpr uint32_t Atlas4096 = 4096;
        constexpr uint32_t Atlas8192 = 8192;
        constexpr float ShadowNearPlane = 0.05f;
        constexpr float MaxSpotShadowFov = 170.0f;
        constexpr float PointFaceFov = 90.0f;
        constexpr float LocalShadowSlopeBias = 2.0f;
        constexpr float LocalShadowFadeBand = 0.1f;
        constexpr float VerticalLightThreshold = 0.99f;
        constexpr float FilterRadius = static_cast<float>(BRIGHT_LOCAL_SHADOW_FILTER_RADIUS);
        constexpr RHI::Format AtlasFormat = RHI::Format::Depth32Float;

        constexpr Math::Vector3 FaceDirections[PointLightFaceCount] = {
            { 1.0f, 0.0f, 0.0f },
            { -1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, -1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
            { 0.0f, 0.0f, -1.0f },
        };

        constexpr Math::Vector3 FaceUps[PointLightFaceCount] = {
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, -1.0f },
            { 0.0f, 0.0f, 1.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
        };

        uint32_t AtlasSizeTexels(LocalShadowAtlasSize size)
        {
            return size == LocalShadowAtlasSize::Size4096 ? Atlas4096 : Atlas8192;
        }

        uint32_t ViewCount(const LocalShadowCandidate& candidate)
        {
            return candidate.type == LightType::Point ? PointLightFaceCount : 1;
        }

        float GuardedHalfFovTangent(float fovDegrees, uint32_t tileSize)
        {
            float tangent = std::tan(fovDegrees * Math::DegreesToRadians * 0.5f);
            float size = static_cast<float>(tileSize);
            return tangent * size / (size - 2.0f * FilterRadius);
        }

        float ViewFov(const LocalShadowCandidate& candidate)
        {
            return candidate.type == LightType::Point ? PointFaceFov : std::min(candidate.outerAngle * 2.0f, MaxSpotShadowFov);
        }

        Math::Matrix4 ViewMatrix(const LocalShadowCandidate& candidate, uint32_t face)
        {
            if (candidate.type == LightType::Point)
            {
                return Math::Matrix4::LookAtLH(candidate.position, candidate.position + FaceDirections[face], FaceUps[face]);
            }
            Math::Vector3 up = std::abs(candidate.direction.y) > VerticalLightThreshold ? Math::Vector3::Forward() : Math::Vector3::Up();
            return Math::Matrix4::LookAtLH(candidate.position, candidate.position + candidate.direction, up);
        }

        Math::Bounds ReachBounds(const LocalShadowCandidate& candidate)
        {
            Math::Bounds reach;
            reach.minimum = candidate.position - Math::Vector3(candidate.range);
            reach.maximum = candidate.position + Math::Vector3(candidate.range);
            return reach;
        }

        bool Overlaps(const Math::Bounds& a, const Math::Bounds& b)
        {
            return a.minimum.x <= b.maximum.x && b.minimum.x <= a.maximum.x && a.minimum.y <= b.maximum.y && b.minimum.y <= a.maximum.y && a.minimum.z <= b.maximum.z && b.minimum.z <= a.maximum.z;
        }
    }

    void LocalShadowPass::EnsureAtlas(uint32_t size)
    {
        if (m_atlas && m_atlasSize == size)
        {
            return;
        }
        m_device.WaitIdle();
        RHI::TextureDesc desc;
        desc.width = size;
        desc.height = size;
        desc.format = AtlasFormat;
        desc.usage = RHI::TextureUsageDepthStencil | RHI::TextureUsageSampled;
        desc.debugName = "LocalShadowAtlas";
        m_atlas = m_device.CreateTexture(desc);
        m_atlasSize = size;
    }

    std::vector<LocalShadowPass::PlannedLight> LocalShadowPass::PlanLights(const FrameContext& frame, uint32_t atlasSize)
    {
        const std::vector<LocalShadowCandidate>& candidates = frame.lighting.localShadowCandidates;
        Math::Frustum frustum = Math::Frustum::FromViewProjection(frame.viewProjection);
        std::vector<RankedLocalShadow> ranked = LocalShadowSelection::Rank(candidates, frame.view, frustum);
        m_allocator.Reset(atlasSize);
        std::vector<PlannedLight> planned;
        uint32_t nextView = 0;
        for (const RankedLocalShadow& entry : ranked)
        {
            const LocalShadowCandidate& candidate = candidates[entry.candidate];
            std::vector<ShadowTile> tiles = m_allocator.AllocateTiles(entry.tileSize, ViewCount(candidate));
            if (tiles.empty())
            {
                continue;
            }
            uint32_t count = static_cast<uint32_t>(tiles.size());
            planned.push_back({ &candidate, std::move(tiles), nextView });
            nextView += count;
        }
        return planned;
    }

    void LocalShadowPass::RenderTile(FrameContext& frame, const LocalShadowCandidate& candidate, const Math::Matrix4& viewProjection, const ShadowTile& tile, RHI::Pipeline* pipeline)
    {
        RHI::CommandList& commands = frame.commands;
        RHI::Viewport viewport;
        viewport.x = static_cast<float>(tile.x);
        viewport.y = static_cast<float>(tile.y);
        viewport.width = static_cast<float>(tile.size);
        viewport.height = static_cast<float>(tile.size);
        commands.SetViewport(viewport);
        commands.SetScissor({ static_cast<int32_t>(tile.x), static_cast<int32_t>(tile.y), static_cast<int32_t>(tile.size), static_cast<int32_t>(tile.size) });
        ViewConstants viewConstants{};
        viewConstants.viewProjection = viewProjection;
        viewConstants.unjitteredViewProjection = viewProjection;
        viewConstants.previousViewProjection = viewProjection;
        commands.SetConstants(BRIGHT_SLOT_VIEW_CONSTANTS, viewConstants);
        Math::Frustum frustum = Math::Frustum::FromViewProjection(viewProjection);
        Math::Bounds reach = ReachBounds(candidate);
        std::vector<const DrawItem*> casters;
        for (const DrawItem& item : frame.drawItems)
        {
            if (!item.castShadows)
            {
                continue;
            }
            Math::Bounds bounds = item.mesh->bounds.Transformed(item.world);
            if (Overlaps(bounds, reach) && frustum.Intersects(bounds))
            {
                casters.push_back(&item);
            }
        }
        m_statistics.shadowCasters += static_cast<uint32_t>(casters.size());
        DrawItems(commands, casters, pipeline, pipeline);
    }

    void LocalShadowPass::Execute(FrameContext& frame)
    {
        FrameLighting& lighting = frame.lighting;
        auto* constants = reinterpret_cast<LocalShadowConstants*>(lighting.localShadowConstants.data());
        std::memset(constants, 0, sizeof(LocalShadowConstants));
        lighting.localShadowAtlas = nullptr;
        if (lighting.localShadowCandidates.empty())
        {
            return;
        }
        uint32_t atlasSize = AtlasSizeTexels(frame.view.shadows.localAtlasSize);
        std::vector<PlannedLight> planned = PlanLights(frame, atlasSize);
        if (planned.empty())
        {
            return;
        }
        EnsureAtlas(atlasSize);
        lighting.localShadowAtlas = m_atlas.get();

        auto* lights = reinterpret_cast<LightConstants*>(lighting.lightConstants.data());
        RHI::Pipeline* pipeline = GetMeshPipeline(ShaderProgram::ShadowDepth, RHI::CullMode::None, {}, AtlasFormat, DepthWrite::On, {}, DepthClip::On, 0, LocalShadowSlopeBias);
        RHI::RenderPassDesc pass;
        pass.depth.texture = m_atlas.get();
        pass.depth.load = RHI::LoadAction::Clear;
        pass.debugName = "LocalShadowAtlas";
        frame.commands.BeginRenderPass(pass);
        float atlas = static_cast<float>(atlasSize);
        for (const PlannedLight& light : planned)
        {
            const LocalShadowCandidate& candidate = *light.candidate;
            lights->lights[candidate.lightIndex].spotShadow[3] = static_cast<float>(light.firstView);
            for (uint32_t face = 0; face < light.tiles.size(); ++face)
            {
                const ShadowTile& tile = light.tiles[face];
                float halfFovTangent = GuardedHalfFovTangent(ViewFov(candidate), tile.size);
                Math::Matrix4 projection = Math::Matrix4::PerspectiveFovLH(2.0f * std::atan(halfFovTangent), 1.0f, ShadowNearPlane, candidate.range);
                Math::Matrix4 viewProjection = ViewMatrix(candidate, face) * projection;
                LocalShadowView& view = constants->views[light.firstView + face];
                view.viewProjection = viewProjection;
                Store(view.atlasRect, static_cast<float>(tile.x) / atlas, static_cast<float>(tile.y) / atlas, static_cast<float>(tile.size) / atlas, static_cast<float>(tile.size) / atlas);
                Store(view.params, candidate.bias, candidate.normalBias * 2.0f * halfFovTangent / static_cast<float>(tile.size), ShadowNearPlane, candidate.range);
                RenderTile(frame, candidate, viewProjection, tile, pipeline);
                ++m_statistics.localShadowViews;
            }
            ++m_statistics.localShadowLights;
        }
        frame.commands.EndRenderPass();
        float localDistance = frame.view.shadows.localDistance;
        Store(constants->settings, localDistance, localDistance * (1.0f - LocalShadowFadeBand), 1.0f / atlas, 1.0f);
    }
}
