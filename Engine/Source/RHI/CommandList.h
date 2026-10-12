#pragma once

#include "RHI/Resources.h"

namespace BrightBox3D::RHI
{
    class CommandList
    {
    public:
        virtual ~CommandList() = default;

        virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
        virtual void EndRenderPass() = 0;

        virtual void SetPipeline(Pipeline* pipeline) = 0;
        virtual void SetViewport(const Viewport& viewport) = 0;
        virtual void SetScissor(const ScissorRect& scissor) = 0;
        virtual void SetVertexBuffer(uint32_t slot, Buffer* buffer, size_t offset = 0) = 0;
        virtual void SetIndexBuffer(Buffer* buffer, IndexFormat format, size_t offset = 0) = 0;
        virtual void SetVertexData(uint32_t slot, const void* data, size_t size) = 0;
        virtual void SetIndexData(const void* data, size_t size, IndexFormat format) = 0;
        virtual void SetConstants(uint32_t slot, const void* data, size_t size) = 0;
        virtual void SetConstantBuffer(uint32_t slot, Buffer* buffer) = 0;
        virtual void SetTexture(uint32_t slot, Texture* texture) = 0;
        virtual void SetSampler(uint32_t slot, Sampler* sampler) = 0;

        virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t baseVertex = 0, uint32_t firstInstance = 0) = 0;

        virtual void CopyTexture(Texture* source, Texture* destination) = 0;
        virtual void GenerateMips(Texture* texture) = 0;

        virtual void PushDebugGroup(const char* name) = 0;
        virtual void PopDebugGroup() = 0;

        template <typename T>
        void SetConstants(uint32_t slot, const T& data)
        {
            SetConstants(slot, &data, sizeof(T));
        }
    };
}
