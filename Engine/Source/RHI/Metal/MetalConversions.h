#pragma once

#include "RHI/Types.h"

#import <Metal/Metal.h>

namespace BrightBox3D::RHI
{
    MTLPixelFormat ToMetal(Format format);
    MTLVertexFormat ToMetalVertex(Format format);
    MTLCompareFunction ToMetal(CompareFunction function);
    MTLBlendFactor ToMetal(BlendFactor factor);
    MTLBlendOperation ToMetal(BlendOperation operation);
    MTLSamplerAddressMode ToMetal(AddressMode mode);
    MTLPrimitiveType ToMetal(PrimitiveTopology topology);
    MTLLoadAction ToMetal(LoadAction action);
    MTLCullMode ToMetal(CullMode mode);
    MTLColorWriteMask ToMetalWriteMask(uint32_t writeMask);
    MTLPrimitiveTopologyClass ToMetalTopologyClass(PrimitiveTopology topology);
}
