#include "Rendering/RenderPass.h"

#include "Geometry/MeshData.h"
#include "Rendering/DrawCollector.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/RenderConstants.h"
#include "Rendering/ShaderLibrary.h"
#include "Shaders/ShaderBindings.h"

#include <cstring>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr uint32_t ObjectIdByteMask = 0xFF;
        constexpr float ObjectIdByteScale = 255.0f;

        RHI::VertexLayout MeshLayout()
        {
            RHI::VertexLayout layout;
            layout.attributes = {
                { RHI::VertexSemantic::Position, 0, RHI::Format::RGB32Float, 0, 0 },
                { RHI::VertexSemantic::Normal, 0, RHI::Format::RGB32Float, 12, 0 },
                { RHI::VertexSemantic::Tangent, 0, RHI::Format::RGBA32Float, 24, 0 },
                { RHI::VertexSemantic::TexCoord, 0, RHI::Format::RG32Float, 40, 0 }
            };
            layout.strides[0] = sizeof(Geometry::MeshVertex);
            return layout;
        }

        ObjectConstants MakeObjectConstants(const DrawItem& item)
        {
            ObjectConstants object;
            object.world = item.world;
            object.previousWorld = item.previousWorld;
            object.normal = item.world.Inverse().Transposed();
            Store(object.params, static_cast<float>(item.objectId & ObjectIdByteMask) / ObjectIdByteScale, 0.0f, 0.0f, 0.0f);
            return object;
        }

        MaterialConstants MakeMaterialConstants(const MaterialBinding& binding)
        {
            MaterialConstants material;
            std::memcpy(material.baseColor, binding.baseColor, sizeof(material.baseColor));
            std::memcpy(material.emissive, binding.emissive, sizeof(material.emissive));
            std::memcpy(material.surface, binding.surface, sizeof(material.surface));
            std::memcpy(material.uvTransform, binding.uvTransform, sizeof(material.uvTransform));
            std::memcpy(material.textureFlags, binding.textureFlags, sizeof(material.textureFlags));
            std::memcpy(material.materialFlags, binding.materialFlags, sizeof(material.materialFlags));
            return material;
        }
    }

    RenderPass::RenderPass(const RenderPassResources& resources) : m_device(resources.device), m_shaders(resources.shaders), m_resources(resources.resources), m_statistics(resources.statistics)
    {
    }

    RHI::Pipeline* RenderPass::GetMeshPipeline(ShaderProgram program, RHI::CullMode cull, const std::vector<RHI::Format>& colors, RHI::Format depth, DepthWrite depthWrite, const RHI::BlendState& blend, DepthClip depthClip, int32_t depthBias, float slopeBias)
    {
        PipelineKey key{ program, colors.empty() ? depth : colors.front(), cull, true };
        RHI::PipelineDesc desc;
        desc.vertexShader = m_shaders.Get(program, RHI::ShaderStage::Vertex);
        desc.pixelShader = m_shaders.Get(program, RHI::ShaderStage::Pixel);
        desc.vertexLayout = MeshLayout();
        desc.rasterizer.cullMode = cull;
        desc.rasterizer.depthClip = depthClip == DepthClip::On;
        desc.rasterizer.depthBias = depthBias;
        desc.rasterizer.slopeScaledDepthBias = slopeBias;
        desc.depthStencil.depthTest = true;
        desc.depthStencil.depthWrite = depthWrite == DepthWrite::On;
        desc.colorFormats = colors;
        desc.depthFormat = depth;
        for (size_t i = 0; i < colors.size(); ++i)
        {
            desc.blend[i] = blend;
        }
        return m_shaders.GetPipeline(key, desc);
    }

    RHI::Pipeline* RenderPass::GetFullscreenPipeline(ShaderProgram program, RHI::Format format, const RHI::BlendState& blend)
    {
        RHI::PipelineDesc desc;
        desc.vertexShader = m_shaders.Get(program, RHI::ShaderStage::Vertex);
        desc.pixelShader = m_shaders.Get(program, RHI::ShaderStage::Pixel);
        desc.rasterizer.cullMode = RHI::CullMode::None;
        desc.depthStencil.depthTest = false;
        desc.depthStencil.depthWrite = false;
        desc.colorFormats = { format };
        desc.blend[0] = blend;
        return m_shaders.GetPipeline({ program, format }, desc);
    }

    void RenderPass::DrawItems(RHI::CommandList& commands, const std::vector<const DrawItem*>& items, RHI::Pipeline* single, RHI::Pipeline* doubleSided)
    {
        GpuMesh* boundMesh = nullptr;
        for (const DrawItem* item : items)
        {
            RHI::Pipeline* pipeline = item->material.doubleSided ? doubleSided : single;
            if (!pipeline)
            {
                continue;
            }
            commands.SetPipeline(pipeline);
            if (item->mesh != boundMesh)
            {
                commands.SetVertexBuffer(0, item->mesh->vertexBuffer.get());
                commands.SetIndexBuffer(item->mesh->indexBuffer.get(), RHI::IndexFormat::UInt32);
                boundMesh = item->mesh;
            }
            commands.SetConstants(BRIGHT_SLOT_OBJECT_CONSTANTS, MakeObjectConstants(*item));
            commands.SetConstants(BRIGHT_SLOT_MATERIAL_CONSTANTS, MakeMaterialConstants(item->material));
            for (uint32_t slot = 0; slot < MaterialTextureCount; ++slot)
            {
                commands.SetTexture(BRIGHT_SLOT_BASE_COLOR_MAP + slot, item->material.textures[slot]);
            }
            commands.SetSampler(BRIGHT_SLOT_MATERIAL_SAMPLER, item->material.sampler);
            const Geometry::SubMesh& subMesh = item->mesh->subMeshes[item->subMesh];
            commands.DrawIndexed(subMesh.indexCount, 1, subMesh.indexOffset, 0, 0);
            ++m_statistics.drawCalls;
        }
    }

    void RenderPass::FullscreenPass(RHI::CommandList& commands, RHI::Texture* target, RHI::Pipeline* pipeline, const char* name, RHI::LoadAction load, const std::function<void(RHI::CommandList&)>& bind)
    {
        RHI::RenderPassDesc pass;
        pass.colors.push_back({ target, 0, 0, load });
        pass.debugName = name;
        commands.BeginRenderPass(pass);
        commands.SetPipeline(pipeline);
        bind(commands);
        commands.Draw(FullscreenTriangleVertices);
        commands.EndRenderPass();
    }
}
