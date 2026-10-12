#pragma once

#include "Math/Geometry.h"
#include "RHI/Device.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/RenderView.h"
#include "Rendering/ShaderLibrary.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace BrightBox3D::Level
{
    class Level;
    class Component;
}

namespace BrightBox3D::Rendering
{
    class MaterialAsset;
    struct EmbeddedMaterial;

    struct RenderStatistics
    {
        uint32_t drawCalls = 0;
        uint32_t visibleObjects = 0;
        uint32_t culledObjects = 0;
        uint32_t lights = 0;
        uint32_t shadowCasters = 0;
    };

    class Renderer
    {
    public:
        Renderer(RHI::Device& device, ShaderLibrary& shaders, GpuResourceCache& resources);
        ~Renderer();

        void Render(RHI::CommandList& commands, const Level::Level* level, const RenderView& view, RHI::Texture* output);
        void ReleaseView(uint64_t viewId);
        const RenderStatistics& GetStatistics() const { return m_statistics; }
        RHI::Texture* GetViewDepth(uint64_t viewId) const;

    private:
        struct MaterialBinding
        {
            float baseColor[4] = { 1, 1, 1, 1 };
            float emissive[4] = { 0, 0, 0, 0 };
            float surface[4] = { 0, 0.5f, 1, 0.5f };
            float uvTransform[4] = { 1, 1, 0, 0 };
            float textureFlags[4] = { 0, 0, 0, 0 };
            float materialFlags[4] = { 0, 0, 0, 0 };
            RHI::Texture* textures[5] = {};
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
        };

        struct ViewTargets;

        ViewTargets& GetTargets(const RenderView& view);
        void CollectDraws(const Level::Level* level, const RenderView& view, ViewTargets& targets, const Math::Frustum& frustum);
        void CollectLights(const Level::Level* level, const RenderView& view);
        MaterialBinding ResolveMaterial(const MaterialAsset* material, const EmbeddedMaterial* embedded);
        void RenderShadows(RHI::CommandList& commands, const RenderView& view, ViewTargets& targets);
        void DrawItems(RHI::CommandList& commands, const std::vector<const DrawItem*>& items, RHI::Pipeline* single, RHI::Pipeline* doubleSided);
        void RenderLines(RHI::CommandList& commands, const RenderView& view, ViewTargets& targets, RHI::Texture* output);
        RHI::Pipeline* GetMeshPipeline(const char* key, const char* pixelEntry, const char* file, RHI::CullMode cull, const std::vector<RHI::Format>& colors, RHI::Format depth, bool depthWrite, const RHI::BlendState& blend, bool depthClip = true, int32_t depthBias = 0, float slopeBias = 0.0f);
        RHI::Pipeline* GetFullscreenPipeline(const char* key, const char* file, const char* entry, RHI::Format format, const RHI::BlendState& blend = {});

        RHI::Device& m_device;
        ShaderLibrary& m_shaders;
        GpuResourceCache& m_resources;
        std::unordered_map<uint64_t, std::unique_ptr<ViewTargets>> m_views;
        std::vector<DrawItem> m_drawItems;
        std::vector<uint8_t> m_lightConstants;
        std::vector<uint8_t> m_shadowConstants;
        Math::Vector3 m_shadowDirection;
        bool m_hasShadowLight = false;
        uint32_t m_lightCount = 0;
        std::unique_ptr<RHI::Texture> m_shadowMap;
        uint32_t m_shadowResolution = 0;
        std::unordered_map<std::string, std::unique_ptr<GpuMesh>> m_proceduralMeshes;
        RenderStatistics m_statistics;
    };
}
