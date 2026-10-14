#include "RHI/DX11/DX11ConstantPool.h"

#include <cstring>

namespace BrightBox3D::RHI
{
    void DX11ConstantPool::Initialize(ID3D11Device* device)
    {
        m_device = device;
    }

    ID3D11Buffer* DX11ConstantPool::Write(ID3D11DeviceContext* context, uint32_t slot, const void* data, size_t size)
    {
        size_t bucket = 256;
        while (bucket < size)
        {
            bucket *= 2;
        }
        uint64_t key = (static_cast<uint64_t>(slot) << 32) | bucket;
        ComPtr<ID3D11Buffer>& buffer = m_buffers[key];
        if (!buffer)
        {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = static_cast<UINT>(bucket);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            m_device->CreateBuffer(&desc, nullptr, &buffer);
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            std::memcpy(mapped.pData, data, size);
            context->Unmap(buffer.Get(), 0);
        }
        return buffer.Get();
    }
}
