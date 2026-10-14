#pragma once

#include "RHI/CommandList.h"
#include "RHI/Metal/MetalResources.h"
#include "RHI/Metal/MetalTransientAllocator.h"

#import <Metal/Metal.h>

namespace BrightBox3D::RHI
{
    class MetalCommandList : public CommandList
    {
    public:
        void Begin(id<MTLCommandBuffer> commandBuffer, uint32_t frame, MetalTransientAllocator* transient);
        void End();
        id<MTLCommandBuffer> GetCommandBuffer() const { return m_commandBuffer; }

        void BeginRenderPass(const RenderPassDesc& desc) override;
        void EndRenderPass() override;
        void SetPipeline(Pipeline* pipeline) override;
        void SetViewport(const Viewport& viewport) override;
        void SetScissor(const ScissorRect& scissor) override;
        void SetVertexBuffer(uint32_t slot, Buffer* buffer, size_t offset) override;
        void SetIndexBuffer(Buffer* buffer, IndexFormat format, size_t offset) override;
        void SetVertexData(uint32_t slot, const void* data, size_t size) override;
        void SetIndexData(const void* data, size_t size, IndexFormat format) override;
        void SetConstants(uint32_t slot, const void* data, size_t size) override;
        void SetConstantBuffer(uint32_t slot, Buffer* buffer) override;
        void SetTexture(uint32_t slot, Texture* texture) override;
        void SetSampler(uint32_t slot, Sampler* sampler) override;
        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) override;
        void CopyTexture(Texture* source, Texture* destination) override;
        void GenerateMips(Texture* texture) override;
        void PushDebugGroup(const char* name) override;
        void PopDebugGroup() override;

    private:
        void EndEncoding();
        void ConfigureColorAttachments(MTLRenderPassDescriptor* pass, const RenderPassDesc& desc, uint32_t& width, uint32_t& height) const;
        void ConfigureDepthAttachment(MTLRenderPassDescriptor* pass, const RenderPassDesc& desc, uint32_t& width, uint32_t& height) const;
        void BindToBothStages(uint32_t slot, id<MTLBuffer> buffer, size_t offset);

        id<MTLCommandBuffer> m_commandBuffer;
        id<MTLRenderCommandEncoder> m_encoder;
        MetalPipeline* m_pipeline = nullptr;
        id<MTLBuffer> m_indexBuffer;
        size_t m_indexOffset = 0;
        IndexFormat m_indexFormat = IndexFormat::UInt32;
        MetalTransientAllocator* m_transient = nullptr;
        uint32_t m_frame = 0;
        uint32_t m_passWidth = 0;
        uint32_t m_passHeight = 0;
    };
}
