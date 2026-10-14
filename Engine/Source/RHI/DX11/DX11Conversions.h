#pragma once

#include "RHI/Types.h"

#include <d3d11_1.h>

namespace BrightBox3D::RHI
{
    DXGI_FORMAT ToDXGI(Format format);
    DXGI_FORMAT TypelessDepth(Format format);
    DXGI_FORMAT DepthShaderView(Format format);
    D3D11_COMPARISON_FUNC ToD3D(CompareFunction function);
    D3D11_BLEND ToD3D(BlendFactor factor);
    D3D11_BLEND_OP ToD3D(BlendOperation operation);
    D3D11_TEXTURE_ADDRESS_MODE ToD3D(AddressMode mode);
    D3D11_PRIMITIVE_TOPOLOGY ToD3D(PrimitiveTopology topology);
}
