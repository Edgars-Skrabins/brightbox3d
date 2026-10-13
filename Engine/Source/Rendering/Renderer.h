#pragma once

#include "Math/Geometry.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/RenderView.h"
#include "Rendering/ShaderLibrary.h"
#include "RHI/Device.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace BrightBox3D::Geometry
{
    struct EmbeddedMaterial;
}

namespace BrightBox3D::Level
{
    class Level;
    class LevelObjectComponent;
}

namespace BrightBox3D::Rendering
{
    class MaterialAsset;

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
        struct FrameContext;

        ViewTargets& GetTargets(const RenderView& view);
        void CollectDraws(const Level::Level* level, const RenderView& view, ViewTargets& targets, const Math::Frustum& frustum);
        void CollectLights(const Level::Level* level, const RenderView& view);
        MaterialBinding ResolveMaterial(const MaterialAsset* material, const Geometry::EmbeddedMaterial* embedded);
        static bool UsesPostAntiAliasing(const RenderView& view);
        void PrepareFrame(FrameContext& frame, const Level::Level* level);
        void RenderShadows(FrameContext& frame);
        void RenderGBuffer(FrameContext& frame);
        void RenderSSAO(FrameContext& frame);
        void BindLighting(FrameContext& frame);
        void RenderLighting(FrameContext& frame);
        void RenderTransparent(FrameContext& frame);
        void RenderTonemap(FrameContext& frame);
        void RenderAntiAliasing(FrameContext& frame);
        void DrawItems(RHI::CommandList& commands, const std::vector<const DrawItem*>& items, RHI::Pipeline* single, RHI::Pipeline* doubleSided);
        void RenderLines(FrameContext& frame);
        RHI::Pipeline* GetMeshPipeline(ShaderProgram program, RHI::CullMode cull, const std::vector<RHI::Format>& colors, RHI::Format depth, bool depthWrite, const RHI::BlendState& blend, bool depthClip = true, int32_t depthBias = 0, float slopeBias = 0.0f);
        RHI::Pipeline* GetFullscreenPipeline(ShaderProgram program, RHI::Format format, const RHI::BlendState& blend = {});
        RHI::Pipeline* GetSMAAPipeline(ShaderProgram program, RHI::Format format);
        void FullscreenPass(RHI::CommandList& commands, RHI::Texture* target, RHI::Pipeline* pipeline, const char* name, bool load, const std::function<void(RHI::CommandList&)>& bind);
        void EnsurePostTargets(const RenderView& view, ViewTargets& targets);
        void EnsureSMAATextures();
        RHI::Texture* RenderPostProcessing(FrameContext& frame);

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
        std::unique_ptr<RHI::Texture> m_smaaArea;
        std::unique_ptr<RHI::Texture> m_smaaSearch;
        float m_ssaoKernel[16][4] = {};
        uint32_t m_shadowResolution = 0;
        std::unordered_map<std::string, std::unique_ptr<GpuMesh>> m_proceduralMeshes;
        RenderStatistics m_statistics;
        RHI::Texture* m_postBloom = nullptr;
        RHI::Texture* m_postExposure = nullptr;
        bool m_postAutoExposure = false;
    };
}
