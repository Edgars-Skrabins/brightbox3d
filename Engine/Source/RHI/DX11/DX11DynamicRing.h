#pragma once

#include "RHI/DX11/DX11Resources.h"

#include <d3d11_1.h>
#include <wrl/client.h>

#include <cstddef>

namespace BrightBox3D::RHI
{
    class DX11DynamicRing
    {
    public:
        void Initialize(ID3D11Device* device, D3D11_BIND_FLAG bind, size_t size);
        ID3D11Buffer* Write(ID3D11DeviceContext* context, const void* data, size_t size, size_t alignment, UINT& offset);

    private:
        ComPtr<ID3D11Buffer> m_buffer;
        size_t m_size = 0;
        size_t m_offset = 0;
    };
}
