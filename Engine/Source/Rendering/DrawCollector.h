#pragma once

#include "Math/Geometry.h"
#include "Math/Matrix4.h"
#include "Rendering/RenderStatistics.h"

#include <cstdint>
#include <vector>

namespace BrightBox3D::Geometry
{
    struct EmbeddedMaterial;
}

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::RHI
{
    class Sampler;
    class Texture;
}

namespace BrightBox3D::Rendering
{
    class GpuResourceCache;
    class MaterialAsset;
    struct FrameLighting;
    struct GpuMesh;
    struct RenderTargets;
    struct RenderView;

    constexpr uint32_t MaterialTextureCount = 5;

    struct MaterialBinding
    {
        float baseColor[4] = { 1, 1, 1, 1 };
        float emissive[4] = { 0, 0, 0, 0 };
        float surface[4] = { 0, 0.5f, 1, 0.5f };
        float uvTransform[4] = { 1, 1, 0, 0 };
        float textureFlags[4] = { 0, 0, 0, 0 };
        float materialFlags[4] = { 0, 0, 0, 0 };
        RHI::Texture* textures[MaterialTextureCount] = {};
        RHI::Sampler* sampler = nullptr;
        bool transparent = false;
        bool doubleSided = false;
    };

    struct DrawItem
    {
        GpuMesh* mesh = nullptr;
        uint32_t subMesh = 0;
        Math::Matrix4 world;
        Math::Matrix4 previousWorld;
        MaterialBinding material;
        float distance = 0.0f;
        bool castShadows = true;
        uint32_t objectId = 0;

        bool IsVisible() const { return distance >= 0.0f; }
    };

    class DrawCollector
    {
    public:
        DrawCollector(GpuResourceCache& resources, RenderStatistics& statistics);

        void CollectDraws(const Level::Level* level, const RenderView& view, RenderTargets& targets, const Math::Frustum& frustum);
        void CollectLights(const Level::Level* level, const RenderView& view, FrameLighting& lighting);
        const std::vector<DrawItem>& GetDrawItems() const { return m_drawItems; }

    private:
        MaterialBinding ResolveMaterial(const MaterialAsset* material, const Geometry::EmbeddedMaterial* embedded);

        GpuResourceCache& m_resources;
        RenderStatistics& m_statistics;
        std::vector<DrawItem> m_drawItems;
    };
}
