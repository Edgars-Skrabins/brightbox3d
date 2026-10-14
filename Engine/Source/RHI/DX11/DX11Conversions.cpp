#include "RHI/DX11/DX11Conversions.h"

namespace BrightBox3D::RHI
{
    DXGI_FORMAT ToDXGI(Format format)
    {
        switch (format)
        {
        case Format::R8Unorm:
            return DXGI_FORMAT_R8_UNORM;
        case Format::RG8Unorm:
            return DXGI_FORMAT_R8G8_UNORM;
        case Format::RGBA8Unorm:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA8Srgb:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case Format::BGRA8Unorm:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case Format::BGRA8Srgb:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case Format::R16Float:
            return DXGI_FORMAT_R16_FLOAT;
        case Format::RG16Float:
            return DXGI_FORMAT_R16G16_FLOAT;
        case Format::RGBA16Float:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case Format::R32Float:
            return DXGI_FORMAT_R32_FLOAT;
        case Format::RG32Float:
            return DXGI_FORMAT_R32G32_FLOAT;
        case Format::RGB32Float:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case Format::RGBA32Float:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case Format::R32Uint:
            return DXGI_FORMAT_R32_UINT;
        case Format::RG11B10Float:
            return DXGI_FORMAT_R11G11B10_FLOAT;
        case Format::RGB10A2Unorm:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        case Format::Depth32Float:
            return DXGI_FORMAT_D32_FLOAT;
        case Format::Depth24Stencil8:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }

    DXGI_FORMAT TypelessDepth(Format format)
    {
        return format == Format::Depth32Float ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_R24G8_TYPELESS;
    }

    DXGI_FORMAT DepthShaderView(Format format)
    {
        return format == Format::Depth32Float ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    }

    D3D11_COMPARISON_FUNC ToD3D(CompareFunction function)
    {
        switch (function)
        {
        case CompareFunction::Never:
            return D3D11_COMPARISON_NEVER;
        case CompareFunction::Less:
            return D3D11_COMPARISON_LESS;
        case CompareFunction::Equal:
            return D3D11_COMPARISON_EQUAL;
        case CompareFunction::LessEqual:
            return D3D11_COMPARISON_LESS_EQUAL;
        case CompareFunction::Greater:
            return D3D11_COMPARISON_GREATER;
        case CompareFunction::NotEqual:
            return D3D11_COMPARISON_NOT_EQUAL;
        case CompareFunction::GreaterEqual:
            return D3D11_COMPARISON_GREATER_EQUAL;
        default:
            return D3D11_COMPARISON_ALWAYS;
        }
    }

    D3D11_BLEND ToD3D(BlendFactor factor)
    {
        switch (factor)
        {
        case BlendFactor::Zero:
            return D3D11_BLEND_ZERO;
        case BlendFactor::One:
            return D3D11_BLEND_ONE;
        case BlendFactor::SourceColor:
            return D3D11_BLEND_SRC_COLOR;
        case BlendFactor::InverseSourceColor:
            return D3D11_BLEND_INV_SRC_COLOR;
        case BlendFactor::SourceAlpha:
            return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::InverseSourceAlpha:
            return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DestinationColor:
            return D3D11_BLEND_DEST_COLOR;
        case BlendFactor::InverseDestinationColor:
            return D3D11_BLEND_INV_DEST_COLOR;
        case BlendFactor::DestinationAlpha:
            return D3D11_BLEND_DEST_ALPHA;
        default:
            return D3D11_BLEND_INV_DEST_ALPHA;
        }
    }

    D3D11_BLEND_OP ToD3D(BlendOperation operation)
    {
        switch (operation)
        {
        case BlendOperation::Subtract:
            return D3D11_BLEND_OP_SUBTRACT;
        case BlendOperation::ReverseSubtract:
            return D3D11_BLEND_OP_REV_SUBTRACT;
        case BlendOperation::Min:
            return D3D11_BLEND_OP_MIN;
        case BlendOperation::Max:
            return D3D11_BLEND_OP_MAX;
        default:
            return D3D11_BLEND_OP_ADD;
        }
    }

    D3D11_TEXTURE_ADDRESS_MODE ToD3D(AddressMode mode)
    {
        switch (mode)
        {
        case AddressMode::Clamp:
            return D3D11_TEXTURE_ADDRESS_CLAMP;
        case AddressMode::Mirror:
            return D3D11_TEXTURE_ADDRESS_MIRROR;
        case AddressMode::Border:
            return D3D11_TEXTURE_ADDRESS_BORDER;
        default:
            return D3D11_TEXTURE_ADDRESS_WRAP;
        }
    }

    D3D11_PRIMITIVE_TOPOLOGY ToD3D(PrimitiveTopology topology)
    {
        switch (topology)
        {
        case PrimitiveTopology::TriangleStrip:
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case PrimitiveTopology::LineList:
            return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveTopology::LineStrip:
            return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveTopology::PointList:
            return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        default:
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }
}
