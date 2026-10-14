#include "RHI/DX11/DX11DynamicRing.h"

#include <cstring>

namespace BrightBox3D::RHI
{
    void DX11DynamicRing::Initialize(ID3D11Device* device, D3D11_BIND_FLAG bind, size_t size)
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(size);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = bind;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&desc, nullptr, &m_buffer);
        m_size = size;
        m_offset = size;
    }

    ID3D11Buffer* DX11DynamicRing::Write(ID3D11DeviceContext* context, const void* data, size_t size, size_t alignment, UINT& offset)
    {
        size_t aligned = (m_offset + alignment - 1) / alignment * alignment;
        D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
        if (aligned + size > m_size)
        {
            aligned = 0;
            mode = D3D11_MAP_WRITE_DISCARD;
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(m_buffer.Get(), 0, mode, 0, &mapped)))
        {
            offset = 0;
            return nullptr;
        }
        std::memcpy(static_cast<uint8_t*>(mapped.pData) + aligned, data, size);
        context->Unmap(m_buffer.Get(), 0);
        m_offset = aligned + size;
        offset = static_cast<UINT>(aligned);
        return m_buffer.Get();
    }
}
