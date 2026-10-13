#pragma once

#include "Core/Event.h"
#include "Core/Guid.h"
#include "Geometry/MeshData.h"
#include "Math/Geometry.h"
#include "Rendering/TextureAsset.h"
#include "RHI/Device.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace BrightBox3D::Rendering
{
    struct GpuMesh
    {
        std::unique_ptr<RHI::Buffer> vertexBuffer;
        std::unique_ptr<RHI::Buffer> indexBuffer;
        std::vector<Geometry::SubMesh> subMeshes;
        Math::Bounds bounds;
        uint32_t indexCount = 0;
    };

    struct GpuTexture
    {
        std::unique_ptr<RHI::Texture> texture;
        TextureFilterMode filter = TextureFilterMode::Trilinear;
        TextureWrapMode wrap = TextureWrapMode::Repeat;
    };

    class GpuResourceCache
    {
    public:
        explicit GpuResourceCache(RHI::Device& device);
        ~GpuResourceCache();

        GpuMesh* GetMesh(const Core::Guid& model, int32_t meshIndex);
        GpuMesh* GetMesh(const Geometry::MeshData& mesh, uint64_t key);
        GpuTexture* GetTexture(const Core::Guid& texture);
        RHI::Texture* GetWhiteTexture() const { return m_white.get(); }
        RHI::Texture* GetBlackTexture() const { return m_black.get(); }
        RHI::Texture* GetFlatNormalTexture() const { return m_flatNormal.get(); }
        RHI::Texture* GetDummyShadowMap() const { return m_dummyShadow.get(); }
        RHI::Sampler* GetSampler(TextureFilterMode filter, TextureWrapMode wrap);
        RHI::Sampler* GetLinearClampSampler();
        RHI::Sampler* GetPointClampSampler();
        RHI::Sampler* GetShadowSampler();

        void Invalidate(const Core::Guid& asset);
        void Clear();
        void ProcessInvalidations();

    private:
        std::unique_ptr<RHI::Texture> CreateSolidTexture(uint32_t rgba, const char* name);
        std::unique_ptr<GpuMesh> Upload(const Geometry::MeshData& mesh);

        RHI::Device& m_device;
        std::unordered_map<uint64_t, std::unique_ptr<GpuMesh>> m_meshes;
        std::unordered_map<Core::Guid, std::unique_ptr<GpuTexture>> m_textures;
        std::unordered_map<uint32_t, std::unique_ptr<RHI::Sampler>> m_samplers;
        std::unique_ptr<RHI::Sampler> m_linearClamp;
        std::unique_ptr<RHI::Sampler> m_pointClamp;
        std::unique_ptr<RHI::Sampler> m_shadowSampler;
        std::unique_ptr<RHI::Texture> m_white;
        std::unique_ptr<RHI::Texture> m_black;
        std::unique_ptr<RHI::Texture> m_flatNormal;
        std::unique_ptr<RHI::Texture> m_dummyShadow;
        std::vector<Core::Guid> m_pendingInvalidations;
        Core::EventHandle m_assetChangedHandle = 0;
        Core::EventHandle m_assetRemovedHandle = 0;
        std::unordered_map<Core::Guid, std::vector<uint64_t>> m_meshKeysByAsset;
    };
}
