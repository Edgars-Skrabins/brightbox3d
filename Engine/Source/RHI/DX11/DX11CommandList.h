#pragma once

#include "RHI/CommandList.h"
#include "RHI/DX11/DX11ConstantPool.h"
#include "RHI/DX11/DX11DynamicRing.h"
#include "RHI/DX11/DX11Resources.h"

#include <d3d11_1.h>
#include <wrl/client.h>

namespace BrightBox3D::RHI
{
    class DX11CommandList : public CommandList
    {
    public:
        void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

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

        void ResetState();

    private:
        ID3D11Device* m_device = nullptr;
        ComPtr<ID3D11DeviceContext> m_context;
        ComPtr<ID3DUserDefinedAnnotation> m_annotation;
        DX11Pipeline* m_pipeline = nullptr;
        DX11DynamicRing m_vertexRing;
        DX11DynamicRing m_indexRing;
        DX11ConstantPool m_constants;
        uint32_t m_passWidth = 0;
        uint32_t m_passHeight = 0;
        uint32_t m_indexSize = 4;
        bool m_inPassEvent = false;
    };
}
