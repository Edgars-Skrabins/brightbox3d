#include "RHI/Types.h"

namespace BrightBox3D::RHI
{
    bool IsDepthFormat(Format format)
    {
        return format == Format::Depth32Float || format == Format::Depth24Stencil8;
    }

    uint32_t FormatBytesPerPixel(Format format)
    {
        switch (format)
        {
        case Format::R8Unorm:
            return 1;
        case Format::RG8Unorm:
        case Format::R16Float:
            return 2;
        case Format::RGBA8Unorm:
        case Format::RGBA8Srgb:
        case Format::BGRA8Unorm:
        case Format::BGRA8Srgb:
        case Format::RG16Float:
        case Format::R32Float:
        case Format::R32Uint:
        case Format::RG11B10Float:
        case Format::RGB10A2Unorm:
        case Format::Depth32Float:
        case Format::Depth24Stencil8:
            return 4;
        case Format::RGBA16Float:
        case Format::RG32Float:
            return 8;
        case Format::RGB32Float:
            return 12;
        case Format::RGBA32Float:
            return 16;
        default:
            return 0;
        }
    }
}
