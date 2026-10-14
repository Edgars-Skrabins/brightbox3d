#pragma once

#include "RHI/Metal/MetalResources.h"

#import <Metal/Metal.h>

#include <cstddef>
#include <vector>

namespace BrightBox3D::RHI
{
    struct MetalTransientAllocation
    {
        id<MTLBuffer> buffer;
        size_t offset = 0;
    };

    class MetalTransientAllocator
    {
    public:
        void Initialize(id<MTLDevice> device) { m_device = device; }
        void BeginFrame(uint32_t frame);
        MetalTransientAllocation Allocate(const void* data, size_t size);

    private:
        id<MTLBuffer> CreatePage(size_t size) const;

        id<MTLDevice> m_device;
        std::vector<id<MTLBuffer>> m_pages[FramesInFlight];
        uint32_t m_frame = 0;
        size_t m_page = 0;
        size_t m_offset = 0;
    };
}
