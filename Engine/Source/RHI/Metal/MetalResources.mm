#include "RHI/Metal/MetalResources.h"

#include "RHI/Metal/MetalConversions.h"

#include <algorithm>
#include <cstring>

namespace BrightBox3D::RHI
{
    namespace
    {
        constexpr size_t MinimumBufferSize = 16;
        constexpr uint32_t MaxAnisotropy = 16;

        NSString* ToNSString(const std::string& text)
        {
            return [NSString stringWithUTF8String:text.c_str()];
        }

        MTLTextureType ToMetal(TextureType type)
        {
            switch (type)
            {
            case TextureType::TextureCube:
                return MTLTextureTypeCube;
            case TextureType::Texture2DArray:
                return MTLTextureType2DArray;
            default:
                return MTLTextureType2D;
            }
        }

        MTLTextureUsage ToMetalUsage(uint32_t usage)
        {
            MTLTextureUsage metal = 0;
            if (usage & TextureUsageSampled)
            {
                metal |= MTLTextureUsageShaderRead;
            }
            if (usage & (TextureUsageRenderTarget | TextureUsageDepthStencil))
            {
                metal |= MTLTextureUsageRenderTarget;
            }
            return metal;
        }
    }

    MetalBuffer::MetalBuffer(id<MTLDevice> device, const BufferDesc& desc, const void* initialData) : Buffer(desc)
    {
        size_t size = std::max<size_t>(desc.size, MinimumBufferSize);
        uint32_t copies = desc.dynamic ? FramesInFlight : 1;
        for (uint32_t i = 0; i < copies; ++i)
        {
            id<MTLBuffer> buffer = [device newBufferWithLength:size options:MTLResourceStorageModeShared];
            if (initialData)
            {
                std::memcpy([buffer contents], initialData, desc.size);
            }
            if (!desc.debugName.empty())
            {
                buffer.label = ToNSString(desc.debugName);
            }
            m_buffers[i] = buffer;
        }
    }

    MetalTexture::MetalTexture(id<MTLDevice> device, const TextureDesc& desc) : Texture(desc)
    {
        MTLTextureDescriptor* descriptor = [[MTLTextureDescriptor alloc] init];
        descriptor.textureType = ToMetal(desc.type);
        if (desc.type == TextureType::Texture2DArray)
        {
            descriptor.arrayLength = desc.layers;
        }
        descriptor.pixelFormat = ToMetal(desc.format);
        descriptor.width = desc.width;
        descriptor.height = desc.height;
        descriptor.mipmapLevelCount = std::max<uint32_t>(desc.mipLevels, 1);
        descriptor.usage = ToMetalUsage(desc.usage);
        bool renderable = (desc.usage & (TextureUsageRenderTarget | TextureUsageDepthStencil)) != 0;
        descriptor.storageMode = renderable || IsDepthFormat(desc.format) ? MTLStorageModePrivate : MTLStorageModeShared;
        m_texture = [device newTextureWithDescriptor:descriptor];
        if (!desc.debugName.empty())
        {
            m_texture.label = ToNSString(desc.debugName);
        }
    }

    MetalSampler::MetalSampler(id<MTLDevice> device, const SamplerDesc& desc) : Sampler(desc)
    {
        MTLSamplerDescriptor* descriptor = [[MTLSamplerDescriptor alloc] init];
        MTLSamplerMinMagFilter filter = desc.filter == Filter::Point ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
        descriptor.minFilter = filter;
        descriptor.magFilter = filter;
        descriptor.mipFilter = desc.mipLinear ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
        descriptor.sAddressMode = ToMetal(desc.addressU);
        descriptor.tAddressMode = ToMetal(desc.addressV);
        descriptor.rAddressMode = ToMetal(desc.addressW);
        descriptor.maxAnisotropy = desc.filter == Filter::Anisotropic ? std::clamp<uint32_t>(desc.maxAnisotropy, 1, MaxAnisotropy) : 1;
        descriptor.borderColor = desc.borderWhite ? MTLSamplerBorderColorOpaqueWhite : MTLSamplerBorderColorTransparentBlack;
        if (desc.comparison)
        {
            descriptor.compareFunction = ToMetal(desc.compare);
        }
        descriptor.supportArgumentBuffers = NO;
        m_sampler = [device newSamplerStateWithDescriptor:descriptor];
    }
}
