#include "RHI/DX11/DX11CommandList.h"

#include "Core/StringUtils.h"
#include "RHI/DX11/DX11Conversions.h"

#include <algorithm>

namespace BrightBox3D::RHI
{
    namespace
    {
        constexpr size_t TransientRingSize = 16 * 1024 * 1024;
    }

    void DX11CommandList::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        m_device = device;
        m_context = context;
        m_context.As(&m_annotation);
        m_vertexRing.Initialize(device, D3D11_BIND_VERTEX_BUFFER, TransientRingSize);
        m_indexRing.Initialize(device, D3D11_BIND_INDEX_BUFFER, TransientRingSize / 2);
        m_constants.Initialize(device);
    }

    void DX11CommandList::BeginRenderPass(const RenderPassDesc& desc)
    {
        ID3D11ShaderResourceView* nullViews[MaxTextureSlots] = {};
        m_context->PSSetShaderResources(0, MaxTextureSlots, nullViews);
        m_context->VSSetShaderResources(0, MaxTextureSlots, nullViews);

        ID3D11RenderTargetView* targets[MaxColorTargets] = {};
        UINT count = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        for (size_t i = 0; i < desc.colors.size() && i < MaxColorTargets; ++i)
        {
            auto* texture = static_cast<DX11Texture*>(desc.colors[i].texture);
            if (!texture)
            {
                continue;
            }
            targets[i] = texture->GetRenderView(m_device, desc.colors[i].mipLevel, desc.colors[i].layer);
            count = static_cast<UINT>(i + 1);
            if (desc.colors[i].load == LoadAction::Clear)
            {
                m_context->ClearRenderTargetView(targets[i], desc.colors[i].clearColor.data());
            }
            width = std::max<uint32_t>(texture->GetWidth() >> desc.colors[i].mipLevel, 1);
            height = std::max<uint32_t>(texture->GetHeight() >> desc.colors[i].mipLevel, 1);
        }
        ID3D11DepthStencilView* depthView = nullptr;
        if (auto* depth = static_cast<DX11Texture*>(desc.depth.texture))
        {
            depthView = depth->GetDepthView(m_device, desc.depth.layer);
            if (desc.depth.load == LoadAction::Clear)
            {
                m_context->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, desc.depth.clearDepth, 0);
            }
            if (width == 0)
            {
                width = depth->GetWidth();
                height = depth->GetHeight();
            }
        }
        m_context->OMSetRenderTargets(count, targets, depthView);
        m_passWidth = width;
        m_passHeight = height;
        m_pipeline = nullptr;
        Viewport viewport;
        viewport.width = static_cast<float>(width);
        viewport.height = static_cast<float>(height);
        SetViewport(viewport);
        if (m_annotation && !desc.debugName.empty())
        {
            m_annotation->BeginEvent(Core::Widen(desc.debugName).c_str());
            m_inPassEvent = true;
        }
    }

    void DX11CommandList::EndRenderPass()
    {
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        if (m_inPassEvent && m_annotation)
        {
            m_annotation->EndEvent();
            m_inPassEvent = false;
        }
    }

    void DX11CommandList::SetPipeline(Pipeline* pipeline)
    {
        auto* dx = static_cast<DX11Pipeline*>(pipeline);
        if (!dx || dx == m_pipeline)
        {
            return;
        }
        m_pipeline = dx;
        const PipelineDesc& desc = dx->GetDesc();
        auto* vertex = static_cast<DX11Shader*>(desc.vertexShader);
        auto* pixel = static_cast<DX11Shader*>(desc.pixelShader);
        m_context->VSSetShader(vertex ? vertex->vertex.Get() : nullptr, nullptr, 0);
        m_context->PSSetShader(pixel ? pixel->pixel.Get() : nullptr, nullptr, 0);
        m_context->IASetInputLayout(dx->inputLayout.Get());
        m_context->IASetPrimitiveTopology(ToD3D(desc.topology));
        m_context->RSSetState(dx->rasterizer.Get());
        m_context->OMSetDepthStencilState(dx->depthStencil.Get(), 0);
        float blendFactor[4] = { 1, 1, 1, 1 };
        m_context->OMSetBlendState(dx->blend.Get(), blendFactor, 0xFFFFFFFF);
        if (!desc.rasterizer.scissor)
        {
            D3D11_RECT rect = { 0, 0, static_cast<LONG>(m_passWidth), static_cast<LONG>(m_passHeight) };
            m_context->RSSetScissorRects(1, &rect);
        }
    }

    void DX11CommandList::SetViewport(const Viewport& viewport)
    {
        D3D11_VIEWPORT dx = { viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth };
        m_context->RSSetViewports(1, &dx);
    }

    void DX11CommandList::SetScissor(const ScissorRect& scissor)
    {
        D3D11_RECT rect = { scissor.x, scissor.y, scissor.x + scissor.width, scissor.y + scissor.height };
        m_context->RSSetScissorRects(1, &rect);
    }

    void DX11CommandList::SetVertexBuffer(uint32_t slot, Buffer* buffer, size_t offset)
    {
        ID3D11Buffer* dx = buffer ? static_cast<DX11Buffer*>(buffer)->Get() : nullptr;
        UINT stride = m_pipeline ? m_pipeline->GetDesc().vertexLayout.strides[slot] : 0;
        UINT dxOffset = static_cast<UINT>(offset);
        m_context->IASetVertexBuffers(slot, 1, &dx, &stride, &dxOffset);
    }

    void DX11CommandList::SetIndexBuffer(Buffer* buffer, IndexFormat format, size_t offset)
    {
        ID3D11Buffer* dx = buffer ? static_cast<DX11Buffer*>(buffer)->Get() : nullptr;
        m_indexSize = format == IndexFormat::UInt16 ? 2 : 4;
        m_context->IASetIndexBuffer(dx, format == IndexFormat::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, static_cast<UINT>(offset));
    }

    void DX11CommandList::SetVertexData(uint32_t slot, const void* data, size_t size)
    {
        UINT stride = m_pipeline ? m_pipeline->GetDesc().vertexLayout.strides[slot] : 4;
        UINT offset = 0;
        ID3D11Buffer* buffer = m_vertexRing.Write(m_context.Get(), data, size, std::max<UINT>(stride, 4), offset);
        m_context->IASetVertexBuffers(slot, 1, &buffer, &stride, &offset);
    }

    void DX11CommandList::SetIndexData(const void* data, size_t size, IndexFormat format)
    {
        UINT offset = 0;
        ID3D11Buffer* buffer = m_indexRing.Write(m_context.Get(), data, size, 4, offset);
        m_indexSize = format == IndexFormat::UInt16 ? 2 : 4;
        m_context->IASetIndexBuffer(buffer, format == IndexFormat::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, offset);
    }

    void DX11CommandList::SetConstants(uint32_t slot, const void* data, size_t size)
    {
        ID3D11Buffer* buffer = m_constants.Write(m_context.Get(), slot, data, size);
        m_context->VSSetConstantBuffers(slot, 1, &buffer);
        m_context->PSSetConstantBuffers(slot, 1, &buffer);
    }

    void DX11CommandList::SetConstantBuffer(uint32_t slot, Buffer* buffer)
    {
        ID3D11Buffer* dx = buffer ? static_cast<DX11Buffer*>(buffer)->Get() : nullptr;
        m_context->VSSetConstantBuffers(slot, 1, &dx);
        m_context->PSSetConstantBuffers(slot, 1, &dx);
    }

    void DX11CommandList::SetTexture(uint32_t slot, Texture* texture)
    {
        ID3D11ShaderResourceView* view = texture ? static_cast<DX11Texture*>(texture)->shaderView.Get() : nullptr;
        m_context->VSSetShaderResources(slot, 1, &view);
        m_context->PSSetShaderResources(slot, 1, &view);
    }

    void DX11CommandList::SetSampler(uint32_t slot, Sampler* sampler)
    {
        ID3D11SamplerState* state = sampler ? static_cast<DX11Sampler*>(sampler)->Get() : nullptr;
        m_context->VSSetSamplers(slot, 1, &state);
        m_context->PSSetSamplers(slot, 1, &state);
    }

    void DX11CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        if (vertexCount == 0)
        {
            return;
        }
        if (instanceCount > 1 || firstInstance > 0)
        {
            m_context->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
        }
        else
        {
            m_context->Draw(vertexCount, firstVertex);
        }
    }

    void DX11CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
    {
        if (indexCount == 0)
        {
            return;
        }
        if (instanceCount > 1 || firstInstance > 0)
        {
            m_context->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
        }
        else
        {
            m_context->DrawIndexed(indexCount, firstIndex, baseVertex);
        }
    }

    void DX11CommandList::CopyTexture(Texture* source, Texture* destination)
    {
        m_context->CopyResource(static_cast<DX11Texture*>(destination)->texture.Get(), static_cast<DX11Texture*>(source)->texture.Get());
    }

    void DX11CommandList::GenerateMips(Texture* texture)
    {
        auto* dx = static_cast<DX11Texture*>(texture);
        if (dx->shaderView)
        {
            m_context->GenerateMips(dx->shaderView.Get());
        }
    }

    void DX11CommandList::PushDebugGroup(const char* name)
    {
        if (m_annotation)
        {
            m_annotation->BeginEvent(Core::Widen(name).c_str());
        }
    }

    void DX11CommandList::PopDebugGroup()
    {
        if (m_annotation)
        {
            m_annotation->EndEvent();
        }
    }

    void DX11CommandList::ResetState()
    {
        m_pipeline = nullptr;
    }
}
