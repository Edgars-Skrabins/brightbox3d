#include "RHI/Metal/MetalConversions.h"

namespace BrightBox3D::RHI
{
    MTLPixelFormat ToMetal(Format format)
    {
        switch (format)
        {
        case Format::R8Unorm:
            return MTLPixelFormatR8Unorm;
        case Format::RG8Unorm:
            return MTLPixelFormatRG8Unorm;
        case Format::RGBA8Unorm:
            return MTLPixelFormatRGBA8Unorm;
        case Format::RGBA8Srgb:
            return MTLPixelFormatRGBA8Unorm_sRGB;
        case Format::BGRA8Unorm:
            return MTLPixelFormatBGRA8Unorm;
        case Format::BGRA8Srgb:
            return MTLPixelFormatBGRA8Unorm_sRGB;
        case Format::R16Float:
            return MTLPixelFormatR16Float;
        case Format::RG16Float:
            return MTLPixelFormatRG16Float;
        case Format::RGBA16Float:
            return MTLPixelFormatRGBA16Float;
        case Format::R32Float:
            return MTLPixelFormatR32Float;
        case Format::RG32Float:
            return MTLPixelFormatRG32Float;
        case Format::RGBA32Float:
            return MTLPixelFormatRGBA32Float;
        case Format::R32Uint:
            return MTLPixelFormatR32Uint;
        case Format::RG11B10Float:
            return MTLPixelFormatRG11B10Float;
        case Format::RGB10A2Unorm:
            return MTLPixelFormatRGB10A2Unorm;
        case Format::Depth32Float:
            return MTLPixelFormatDepth32Float;
        case Format::Depth24Stencil8:
            return MTLPixelFormatDepth32Float_Stencil8;
        default:
            return MTLPixelFormatInvalid;
        }
    }

    MTLVertexFormat ToMetalVertex(Format format)
    {
        switch (format)
        {
        case Format::R32Float:
            return MTLVertexFormatFloat;
        case Format::RG32Float:
            return MTLVertexFormatFloat2;
        case Format::RGB32Float:
            return MTLVertexFormatFloat3;
        case Format::RGBA32Float:
            return MTLVertexFormatFloat4;
        case Format::RGBA8Unorm:
            return MTLVertexFormatUChar4Normalized;
        case Format::R32Uint:
            return MTLVertexFormatUInt;
        case Format::RG16Float:
            return MTLVertexFormatHalf2;
        case Format::RGBA16Float:
            return MTLVertexFormatHalf4;
        default:
            return MTLVertexFormatInvalid;
        }
    }

    MTLCompareFunction ToMetal(CompareFunction function)
    {
        switch (function)
        {
        case CompareFunction::Never:
            return MTLCompareFunctionNever;
        case CompareFunction::Less:
            return MTLCompareFunctionLess;
        case CompareFunction::Equal:
            return MTLCompareFunctionEqual;
        case CompareFunction::LessEqual:
            return MTLCompareFunctionLessEqual;
        case CompareFunction::Greater:
            return MTLCompareFunctionGreater;
        case CompareFunction::NotEqual:
            return MTLCompareFunctionNotEqual;
        case CompareFunction::GreaterEqual:
            return MTLCompareFunctionGreaterEqual;
        default:
            return MTLCompareFunctionAlways;
        }
    }

    MTLBlendFactor ToMetal(BlendFactor factor)
    {
        switch (factor)
        {
        case BlendFactor::Zero:
            return MTLBlendFactorZero;
        case BlendFactor::One:
            return MTLBlendFactorOne;
        case BlendFactor::SourceColor:
            return MTLBlendFactorSourceColor;
        case BlendFactor::InverseSourceColor:
            return MTLBlendFactorOneMinusSourceColor;
        case BlendFactor::SourceAlpha:
            return MTLBlendFactorSourceAlpha;
        case BlendFactor::InverseSourceAlpha:
            return MTLBlendFactorOneMinusSourceAlpha;
        case BlendFactor::DestinationColor:
            return MTLBlendFactorDestinationColor;
        case BlendFactor::InverseDestinationColor:
            return MTLBlendFactorOneMinusDestinationColor;
        case BlendFactor::DestinationAlpha:
            return MTLBlendFactorDestinationAlpha;
        default:
            return MTLBlendFactorOneMinusDestinationAlpha;
        }
    }

    MTLBlendOperation ToMetal(BlendOperation operation)
    {
        switch (operation)
        {
        case BlendOperation::Subtract:
            return MTLBlendOperationSubtract;
        case BlendOperation::ReverseSubtract:
            return MTLBlendOperationReverseSubtract;
        case BlendOperation::Min:
            return MTLBlendOperationMin;
        case BlendOperation::Max:
            return MTLBlendOperationMax;
        default:
            return MTLBlendOperationAdd;
        }
    }

    MTLSamplerAddressMode ToMetal(AddressMode mode)
    {
        switch (mode)
        {
        case AddressMode::Clamp:
            return MTLSamplerAddressModeClampToEdge;
        case AddressMode::Mirror:
            return MTLSamplerAddressModeMirrorRepeat;
        case AddressMode::Border:
            return MTLSamplerAddressModeClampToBorderColor;
        default:
            return MTLSamplerAddressModeRepeat;
        }
    }

    MTLPrimitiveType ToMetal(PrimitiveTopology topology)
    {
        switch (topology)
        {
        case PrimitiveTopology::TriangleStrip:
            return MTLPrimitiveTypeTriangleStrip;
        case PrimitiveTopology::LineList:
            return MTLPrimitiveTypeLine;
        case PrimitiveTopology::LineStrip:
            return MTLPrimitiveTypeLineStrip;
        case PrimitiveTopology::PointList:
            return MTLPrimitiveTypePoint;
        default:
            return MTLPrimitiveTypeTriangle;
        }
    }
    MTLLoadAction ToMetal(LoadAction action)
    {
        switch (action)
        {
        case LoadAction::Clear:
            return MTLLoadActionClear;
        case LoadAction::Load:
            return MTLLoadActionLoad;
        default:
            return MTLLoadActionDontCare;
        }
    }

    MTLCullMode ToMetal(CullMode mode)
    {
        switch (mode)
        {
        case CullMode::None:
            return MTLCullModeNone;
        case CullMode::Front:
            return MTLCullModeFront;
        default:
            return MTLCullModeBack;
        }
    }

    MTLColorWriteMask ToMetalWriteMask(uint32_t writeMask)
    {
        MTLColorWriteMask mask = MTLColorWriteMaskNone;
        if (writeMask & ColorWriteRed)
        {
            mask |= MTLColorWriteMaskRed;
        }
        if (writeMask & ColorWriteGreen)
        {
            mask |= MTLColorWriteMaskGreen;
        }
        if (writeMask & ColorWriteBlue)
        {
            mask |= MTLColorWriteMaskBlue;
        }
        if (writeMask & ColorWriteAlpha)
        {
            mask |= MTLColorWriteMaskAlpha;
        }
        return mask;
    }

    MTLPrimitiveTopologyClass ToMetalTopologyClass(PrimitiveTopology topology)
    {
        switch (topology)
        {
        case PrimitiveTopology::LineList:
        case PrimitiveTopology::LineStrip:
            return MTLPrimitiveTopologyClassLine;
        case PrimitiveTopology::PointList:
            return MTLPrimitiveTopologyClassPoint;
        default:
            return MTLPrimitiveTopologyClassTriangle;
        }
    }
}
