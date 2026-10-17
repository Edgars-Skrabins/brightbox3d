#include "Rendering/GpuResourceCache.h"

#include "Assets/AssetDatabase.h"
#include "Core/Hash.h"
#include "Geometry/ModelAsset.h"

namespace BrightBox3D::Rendering
{
    GpuResourceCache::GpuResourceCache(RHI::Device& device) : m_device(device)
    {
        m_white = CreateSolidTexture(0xFFFFFFFF, "White");
        m_black = CreateSolidTexture(0xFF000000, "Black");
        m_flatNormal = CreateSolidTexture(0xFFFF8080, "FlatNormal");

        RHI::TextureDesc shadowDesc;
        shadowDesc.type = RHI::TextureType::Texture2DArray;
        shadowDesc.width = 1;
        shadowDesc.height = 1;
        shadowDesc.layers = 4;
        shadowDesc.format = RHI::Format::Depth32Float;
        shadowDesc.usage = RHI::TextureUsageDepthStencil | RHI::TextureUsageSampled;
        shadowDesc.debugName = "DummyShadowMap";
        m_dummyShadow = m_device.CreateTexture(shadowDesc);

        m_assetChangedHandle = Assets::AssetDatabase::Get().OnAssetChanged.Subscribe([this](const Core::Guid& guid) { m_pendingInvalidations.push_back(guid); });
        m_assetRemovedHandle = Assets::AssetDatabase::Get().OnAssetRemoved.Subscribe([this](const Core::Guid& guid) { m_pendingInvalidations.push_back(guid); });
    }

    GpuResourceCache::~GpuResourceCache()
    {
        Assets::AssetDatabase::Get().OnAssetChanged.Unsubscribe(m_assetChangedHandle);
        Assets::AssetDatabase::Get().OnAssetRemoved.Unsubscribe(m_assetRemovedHandle);
    }

    std::unique_ptr<RHI::Texture> GpuResourceCache::CreateSolidTexture(uint32_t rgba, const char* name)
    {
        RHI::TextureDesc desc;
        desc.width = 1;
        desc.height = 1;
        desc.format = RHI::Format::RGBA8Unorm;
        desc.debugName = name;
        auto texture = m_device.CreateTexture(desc);
        m_device.UpdateTexture(texture.get(), 0, 0, &rgba, 4);
        return texture;
    }

    std::unique_ptr<GpuMesh> GpuResourceCache::Upload(const Geometry::MeshData& mesh)
    {
        if (mesh.vertices.empty() || mesh.indices.empty())
        {
            return nullptr;
        }
        auto gpu = std::make_unique<GpuMesh>();
        RHI::BufferDesc vertexDesc{ mesh.vertices.size() * sizeof(Geometry::MeshVertex), RHI::BufferUsage::Vertex, false, mesh.name + ".Vertices" };
        RHI::BufferDesc indexDesc{ mesh.indices.size() * sizeof(uint32_t), RHI::BufferUsage::Index, false, mesh.name + ".Indices" };
        gpu->vertexBuffer = m_device.CreateBuffer(vertexDesc, mesh.vertices.data());
        gpu->indexBuffer = m_device.CreateBuffer(indexDesc, mesh.indices.data());
        gpu->subMeshes = mesh.subMeshes;
        if (gpu->subMeshes.empty())
        {
            gpu->subMeshes.push_back({ 0, static_cast<uint32_t>(mesh.indices.size()), 0 });
        }
        gpu->bounds = mesh.bounds;
        gpu->indexCount = static_cast<uint32_t>(mesh.indices.size());
        return gpu;
    }

    GpuMesh* GpuResourceCache::GetMesh(const Core::Guid& model, int32_t meshIndex)
    {
        uint64_t key = Core::HashCombine(Core::HashCombine(model.high, model.low), static_cast<uint64_t>(meshIndex));
        auto it = m_meshes.find(key);
        if (it != m_meshes.end())
        {
            return it->second.get();
        }
        auto asset = Assets::AssetDatabase::Get().Load<Geometry::ModelAsset>(model);
        if (!asset)
        {
            Assets::ReportLoadFailure(asset.GetError());
        }
        const Geometry::MeshData* mesh = asset ? asset->GetMesh(meshIndex) : nullptr;
        std::unique_ptr<GpuMesh> gpu = mesh ? Upload(*mesh) : nullptr;
        GpuMesh* raw = gpu.get();
        m_meshes[key] = std::move(gpu);
        m_meshKeysByAsset[model].push_back(key);
        return raw;
    }

    GpuMesh* GpuResourceCache::GetMesh(const Geometry::MeshData& mesh, uint64_t key)
    {
        auto it = m_meshes.find(key);
        if (it != m_meshes.end())
        {
            return it->second.get();
        }
        std::unique_ptr<GpuMesh> gpu = Upload(mesh);
        GpuMesh* raw = gpu.get();
        m_meshes[key] = std::move(gpu);
        return raw;
    }

    GpuTexture* GpuResourceCache::GetTexture(const Core::Guid& texture)
    {
        if (!texture.IsValid())
        {
            return nullptr;
        }
        auto it = m_textures.find(texture);
        if (it != m_textures.end())
        {
            return it->second.get();
        }
        auto asset = Assets::AssetDatabase::Get().Load<TextureAsset>(texture);
        if (!asset)
        {
            Assets::ReportLoadFailure(asset.GetError());
        }
        std::unique_ptr<GpuTexture> gpu;
        if (asset && !asset->GetMips().empty())
        {
            gpu = std::make_unique<GpuTexture>();
            RHI::TextureDesc desc;
            desc.width = asset->GetWidth();
            desc.height = asset->GetHeight();
            desc.mipLevels = static_cast<uint32_t>(asset->GetMips().size());
            switch (asset->GetPixelFormat())
            {
            case TexturePixelFormat::RGBA8Srgb:
                desc.format = RHI::Format::RGBA8Srgb;
                break;
            case TexturePixelFormat::RGBA32Float:
                desc.format = RHI::Format::RGBA32Float;
                break;
            default:
                desc.format = RHI::Format::RGBA8Unorm;
                break;
            }
            desc.debugName = asset->GetName();
            gpu->texture = m_device.CreateTexture(desc);
            uint32_t bytes = RHI::FormatBytesPerPixel(desc.format);
            for (uint32_t mip = 0; mip < desc.mipLevels; ++mip)
            {
                const TextureMip& data = asset->GetMips()[mip];
                m_device.UpdateTexture(gpu->texture.get(), mip, 0, data.pixels.data(), data.width * bytes);
            }
            gpu->filter = asset->GetFilter();
            gpu->wrap = asset->GetWrap();
        }
        GpuTexture* raw = gpu.get();
        m_textures[texture] = std::move(gpu);
        return raw;
    }

    RHI::Sampler* GpuResourceCache::GetSampler(TextureFilterMode filter, TextureWrapMode wrap)
    {
        uint32_t key = static_cast<uint32_t>(filter) * 16 + static_cast<uint32_t>(wrap);
        auto it = m_samplers.find(key);
        if (it != m_samplers.end())
        {
            return it->second.get();
        }
        RHI::SamplerDesc desc;
        desc.filter = filter == TextureFilterMode::Point ? RHI::Filter::Point : (filter == TextureFilterMode::Anisotropic ? RHI::Filter::Anisotropic : RHI::Filter::Linear);
        desc.mipLinear = filter == TextureFilterMode::Trilinear || filter == TextureFilterMode::Anisotropic;
        desc.maxAnisotropy = 8;
        RHI::AddressMode mode = wrap == TextureWrapMode::Clamp ? RHI::AddressMode::Clamp : (wrap == TextureWrapMode::Mirror ? RHI::AddressMode::Mirror : RHI::AddressMode::Wrap);
        desc.addressU = desc.addressV = desc.addressW = mode;
        auto sampler = m_device.CreateSampler(desc);
        RHI::Sampler* raw = sampler.get();
        m_samplers[key] = std::move(sampler);
        return raw;
    }

    RHI::Sampler* GpuResourceCache::GetLinearClampSampler()
    {
        if (!m_linearClamp)
        {
            RHI::SamplerDesc desc;
            desc.filter = RHI::Filter::Linear;
            desc.addressU = desc.addressV = desc.addressW = RHI::AddressMode::Clamp;
            m_linearClamp = m_device.CreateSampler(desc);
        }
        return m_linearClamp.get();
    }

    RHI::Sampler* GpuResourceCache::GetPointClampSampler()
    {
        if (!m_pointClamp)
        {
            RHI::SamplerDesc desc;
            desc.filter = RHI::Filter::Point;
            desc.mipLinear = false;
            desc.addressU = desc.addressV = desc.addressW = RHI::AddressMode::Clamp;
            m_pointClamp = m_device.CreateSampler(desc);
        }
        return m_pointClamp.get();
    }

    RHI::Sampler* GpuResourceCache::GetShadowSampler()
    {
        if (!m_shadowSampler)
        {
            RHI::SamplerDesc desc;
            desc.filter = RHI::Filter::Linear;
            desc.mipLinear = false;
            desc.addressU = desc.addressV = desc.addressW = RHI::AddressMode::Border;
            desc.borderWhite = true;
            desc.comparison = true;
            desc.compare = RHI::CompareFunction::LessEqual;
            m_shadowSampler = m_device.CreateSampler(desc);
        }
        return m_shadowSampler.get();
    }

    void GpuResourceCache::Invalidate(const Core::Guid& asset)
    {
        m_textures.erase(asset);
        auto keys = m_meshKeysByAsset.find(asset);
        if (keys != m_meshKeysByAsset.end())
        {
            for (uint64_t key : keys->second)
            {
                m_meshes.erase(key);
            }
            m_meshKeysByAsset.erase(keys);
        }
    }

    void GpuResourceCache::ProcessInvalidations()
    {
        if (m_pendingInvalidations.empty())
        {
            return;
        }
        m_device.WaitIdle();
        for (const Core::Guid& guid : m_pendingInvalidations)
        {
            Invalidate(guid);
        }
        m_pendingInvalidations.clear();
    }

    void GpuResourceCache::Clear()
    {
        m_meshes.clear();
        m_textures.clear();
        m_meshKeysByAsset.clear();
    }
}
