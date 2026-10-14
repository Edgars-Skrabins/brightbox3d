#include "RHI/Metal/MetalCommandList.h"

#include "RHI/Metal/MetalConversions.h"

#include <algorithm>

namespace BrightBox3D::RHI
{
    namespace
    {
        constexpr size_t UInt16IndexSize = 2;
        constexpr size_t UInt32IndexSize = 4;

        uint32_t MipExtent(uint32_t extent, uint32_t mipLevel)
        {
            return std::max<uint32_t>(extent >> mipLevel, 1);
        }

        size_t IndexSize(IndexFormat format)
        {
            return format == IndexFormat::UInt16 ? UInt16IndexSize : UInt32IndexSize;
        }

        MTLIndexType ToMetal(IndexFormat format)
        {
            return format == IndexFormat::UInt16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
        }
    }

    void MetalCommandList::Begin(id<MTLCommandBuffer> commandBuffer, uint32_t frame, MetalTransientAllocator* transient)
    {
        m_commandBuffer = commandBuffer;
        m_frame = frame;
        m_transient = transient;
        m_encoder = nil;
        m_pipeline = nullptr;
    }

    void MetalCommandList::End()
    {
        EndEncoding();
        m_commandBuffer = nil;
    }

    void MetalCommandList::ConfigureColorAttachments(MTLRenderPassDescriptor* pass, const RenderPassDesc& desc, uint32_t& width, uint32_t& height) const
    {
        for (size_t i = 0; i < desc.colors.size(); ++i)
        {
            const ColorAttachment& attachment = desc.colors[i];
            auto* texture = static_cast<MetalTexture*>(attachment.texture);
            if (!texture)
            {
                continue;
            }
            MTLRenderPassColorAttachmentDescriptor* color = pass.colorAttachments[i];
            color.texture = texture->Get();
            color.level = attachment.mipLevel;
            color.slice = attachment.layer;
            color.loadAction = ToMetal(attachment.load);
            color.clearColor = MTLClearColorMake(attachment.clearColor[0], attachment.clearColor[1], attachment.clearColor[2], attachment.clearColor[3]);
            color.storeAction = MTLStoreActionStore;
            width = MipExtent(texture->GetWidth(), attachment.mipLevel);
            height = MipExtent(texture->GetHeight(), attachment.mipLevel);
        }
    }

    void MetalCommandList::ConfigureDepthAttachment(MTLRenderPassDescriptor* pass, const RenderPassDesc& desc, uint32_t& width, uint32_t& height) const
    {
        auto* depth = static_cast<MetalTexture*>(desc.depth.texture);
        if (!depth)
        {
            return;
        }
        pass.depthAttachment.texture = depth->Get();
        pass.depthAttachment.slice = desc.depth.layer;
        pass.depthAttachment.loadAction = ToMetal(desc.depth.load);
        pass.depthAttachment.clearDepth = desc.depth.clearDepth;
        pass.depthAttachment.storeAction = MTLStoreActionStore;
        if (depth->GetFormat() == Format::Depth24Stencil8)
        {
            pass.stencilAttachment.texture = depth->Get();
            pass.stencilAttachment.loadAction = MTLLoadActionClear;
            pass.stencilAttachment.storeAction = MTLStoreActionDontCare;
        }
        if (width == 0)
        {
            width = depth->GetWidth();
            height = depth->GetHeight();
        }
    }

    void MetalCommandList::BeginRenderPass(const RenderPassDesc& desc)
    {
        EndEncoding();
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        uint32_t width = 0;
        uint32_t height = 0;
        ConfigureColorAttachments(pass, desc, width, height);
        ConfigureDepthAttachment(pass, desc, width, height);
        m_encoder = [m_commandBuffer renderCommandEncoderWithDescriptor:pass];
        if (!desc.debugName.empty())
        {
            m_encoder.label = [NSString stringWithUTF8String:desc.debugName.c_str()];
        }
        m_passWidth = width;
        m_passHeight = height;
        m_pipeline = nullptr;
        Viewport viewport;
        viewport.width = static_cast<float>(width);
        viewport.height = static_cast<float>(height);
        SetViewport(viewport);
    }

    void MetalCommandList::EndRenderPass()
    {
        EndEncoding();
    }

    void MetalCommandList::SetPipeline(Pipeline* pipeline)
    {
        auto* metal = static_cast<MetalPipeline*>(pipeline);
        if (!m_encoder || !metal || metal == m_pipeline)
        {
            return;
        }
        m_pipeline = metal;
        const RasterizerState& rasterizer = metal->GetDesc().rasterizer;
        [m_encoder setRenderPipelineState:metal->GetState()];
        [m_encoder setDepthStencilState:metal->GetDepthState()];
        [m_encoder setCullMode:ToMetal(rasterizer.cullMode)];
        [m_encoder setFrontFacingWinding:rasterizer.frontCounterClockwise ? MTLWindingCounterClockwise : MTLWindingClockwise];
        [m_encoder setTriangleFillMode:rasterizer.wireframe ? MTLTriangleFillModeLines : MTLTriangleFillModeFill];
        [m_encoder setDepthClipMode:rasterizer.depthClip ? MTLDepthClipModeClip : MTLDepthClipModeClamp];
        [m_encoder setDepthBias:static_cast<float>(rasterizer.depthBias) slopeScale:rasterizer.slopeScaledDepthBias clamp:rasterizer.depthBiasClamp];
        if (!rasterizer.scissor)
        {
            MTLScissorRect full = { 0, 0, m_passWidth, m_passHeight };
            [m_encoder setScissorRect:full];
        }
    }

    void MetalCommandList::SetViewport(const Viewport& viewport)
    {
        if (!m_encoder)
        {
            return;
        }
        MTLViewport metal = { viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth };
        [m_encoder setViewport:metal];
    }

    void MetalCommandList::SetScissor(const ScissorRect& scissor)
    {
        if (!m_encoder)
        {
            return;
        }
        int32_t x = std::clamp<int32_t>(scissor.x, 0, static_cast<int32_t>(m_passWidth));
        int32_t y = std::clamp<int32_t>(scissor.y, 0, static_cast<int32_t>(m_passHeight));
        int32_t right = std::clamp<int32_t>(scissor.x + scissor.width, x, static_cast<int32_t>(m_passWidth));
        int32_t bottom = std::clamp<int32_t>(scissor.y + scissor.height, y, static_cast<int32_t>(m_passHeight));
        MTLScissorRect rect = { static_cast<NSUInteger>(x), static_cast<NSUInteger>(y), static_cast<NSUInteger>(right - x), static_cast<NSUInteger>(bottom - y) };
        [m_encoder setScissorRect:rect];
    }

    void MetalCommandList::SetVertexBuffer(uint32_t slot, Buffer* buffer, size_t offset)
    {
        if (!m_encoder || !buffer)
        {
            return;
        }
        [m_encoder setVertexBuffer:static_cast<MetalBuffer*>(buffer)->Get(m_frame) offset:offset atIndex:VertexBufferBase + slot];
    }

    void MetalCommandList::SetIndexBuffer(Buffer* buffer, IndexFormat format, size_t offset)
    {
        m_indexBuffer = buffer ? static_cast<MetalBuffer*>(buffer)->Get(m_frame) : nil;
        m_indexOffset = offset;
        m_indexFormat = format;
    }

    void MetalCommandList::SetVertexData(uint32_t slot, const void* data, size_t size)
    {
        if (!m_encoder || size == 0)
        {
            return;
        }
        MetalTransientAllocation allocation = m_transient->Allocate(data, size);
        [m_encoder setVertexBuffer:allocation.buffer offset:allocation.offset atIndex:VertexBufferBase + slot];
    }

    void MetalCommandList::SetIndexData(const void* data, size_t size, IndexFormat format)
    {
        if (size == 0)
        {
            return;
        }
        MetalTransientAllocation allocation = m_transient->Allocate(data, size);
        m_indexBuffer = allocation.buffer;
        m_indexOffset = allocation.offset;
        m_indexFormat = format;
    }

    void MetalCommandList::BindToBothStages(uint32_t slot, id<MTLBuffer> buffer, size_t offset)
    {
        [m_encoder setVertexBuffer:buffer offset:offset atIndex:slot];
        [m_encoder setFragmentBuffer:buffer offset:offset atIndex:slot];
    }

    void MetalCommandList::SetConstants(uint32_t slot, const void* data, size_t size)
    {
        if (!m_encoder)
        {
            return;
        }
        MetalTransientAllocation allocation = m_transient->Allocate(data, size);
        BindToBothStages(slot, allocation.buffer, allocation.offset);
    }

    void MetalCommandList::SetConstantBuffer(uint32_t slot, Buffer* buffer)
    {
        if (!m_encoder || !buffer)
        {
            return;
        }
        BindToBothStages(slot, static_cast<MetalBuffer*>(buffer)->Get(m_frame), 0);
    }

    void MetalCommandList::SetTexture(uint32_t slot, Texture* texture)
    {
        if (!m_encoder)
        {
            return;
        }
        id<MTLTexture> metal = texture ? static_cast<MetalTexture*>(texture)->Get() : nil;
        [m_encoder setVertexTexture:metal atIndex:slot];
        [m_encoder setFragmentTexture:metal atIndex:slot];
    }

    void MetalCommandList::SetSampler(uint32_t slot, Sampler* sampler)
    {
        if (!m_encoder || !sampler)
        {
            return;
        }
        id<MTLSamplerState> metal = static_cast<MetalSampler*>(sampler)->Get();
        [m_encoder setVertexSamplerState:metal atIndex:slot];
        [m_encoder setFragmentSamplerState:metal atIndex:slot];
    }

    void MetalCommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        if (!m_encoder || !m_pipeline || vertexCount == 0)
        {
            return;
        }
        [m_encoder drawPrimitives:ToMetal(m_pipeline->GetDesc().topology) vertexStart:firstVertex vertexCount:vertexCount instanceCount:instanceCount baseInstance:firstInstance];
    }

    void MetalCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
    {
        if (!m_encoder || !m_pipeline || !m_indexBuffer || indexCount == 0)
        {
            return;
        }
        [m_encoder drawIndexedPrimitives:ToMetal(m_pipeline->GetDesc().topology)
                              indexCount:indexCount
                               indexType:ToMetal(m_indexFormat)
                             indexBuffer:m_indexBuffer
                       indexBufferOffset:m_indexOffset + firstIndex * IndexSize(m_indexFormat)
                           instanceCount:instanceCount
                              baseVertex:baseVertex
                            baseInstance:firstInstance];
    }

    void MetalCommandList::CopyTexture(Texture* source, Texture* destination)
    {
        EndEncoding();
        id<MTLBlitCommandEncoder> blit = [m_commandBuffer blitCommandEncoder];
        [blit copyFromTexture:static_cast<MetalTexture*>(source)->Get() toTexture:static_cast<MetalTexture*>(destination)->Get()];
        [blit endEncoding];
    }

    void MetalCommandList::GenerateMips(Texture* texture)
    {
        EndEncoding();
        id<MTLBlitCommandEncoder> blit = [m_commandBuffer blitCommandEncoder];
        [blit generateMipmapsForTexture:static_cast<MetalTexture*>(texture)->Get()];
        [blit endEncoding];
    }

    void MetalCommandList::PushDebugGroup(const char* name)
    {
        if (m_encoder)
        {
            [m_encoder pushDebugGroup:[NSString stringWithUTF8String:name]];
        }
    }

    void MetalCommandList::PopDebugGroup()
    {
        if (m_encoder)
        {
            [m_encoder popDebugGroup];
        }
    }

    void MetalCommandList::EndEncoding()
    {
        if (m_encoder)
        {
            [m_encoder endEncoding];
            m_encoder = nil;
        }
        m_pipeline = nullptr;
    }
}
