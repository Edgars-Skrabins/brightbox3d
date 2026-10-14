#include "RHI/Metal/MetalTransientAllocator.h"

#include <algorithm>
#include <cstring>

namespace BrightBox3D::RHI
{
    namespace
    {
        constexpr size_t TransientPageSize = 8 * 1024 * 1024;
        constexpr size_t TransientAlignment = 256;

        size_t AlignUp(size_t value)
        {
            return (value + TransientAlignment - 1) & ~(TransientAlignment - 1);
        }
    }

    id<MTLBuffer> MetalTransientAllocator::CreatePage(size_t size) const
    {
        return [m_device newBufferWithLength:size options:MTLResourceStorageModeShared];
    }

    void MetalTransientAllocator::BeginFrame(uint32_t frame)
    {
        m_frame = frame % FramesInFlight;
        m_pages[m_frame].resize(std::max<size_t>(m_pages[m_frame].size(), 1));
        if (!m_pages[m_frame][0])
        {
            m_pages[m_frame][0] = CreatePage(TransientPageSize);
        }
        m_page = 0;
        m_offset = 0;
    }

    MetalTransientAllocation MetalTransientAllocator::Allocate(const void* data, size_t size)
    {
        size_t aligned = AlignUp(m_offset);
        if (aligned + size > TransientPageSize)
        {
            ++m_page;
            aligned = 0;
            auto& pages = m_pages[m_frame];
            if (m_page >= pages.size())
            {
                pages.push_back(CreatePage(std::max(TransientPageSize, size)));
            }
            else if (pages[m_page].length < size)
            {
                pages[m_page] = CreatePage(size);
            }
        }
        id<MTLBuffer> buffer = m_pages[m_frame][m_page];
        std::memcpy(static_cast<uint8_t*>([buffer contents]) + aligned, data, size);
        m_offset = aligned + size;
        return { buffer, aligned };
    }
}
