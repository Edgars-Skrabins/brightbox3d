#pragma once

#include "RHI/DX11/DX11Resources.h"

#include <d3d11_1.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace BrightBox3D::RHI
{
    class DX11ConstantPool
    {
    public:
        void Initialize(ID3D11Device* device);
        ID3D11Buffer* Write(ID3D11DeviceContext* context, uint32_t slot, const void* data, size_t size);

    private:
        ID3D11Device* m_device = nullptr;
        std::unordered_map<uint64_t, ComPtr<ID3D11Buffer>> m_buffers;
    };
}
