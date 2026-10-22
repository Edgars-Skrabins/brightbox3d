#pragma once

#include <cstdint>

namespace BrightBox3D::Rendering
{
    struct RenderStatistics
    {
        uint32_t drawCalls = 0;
        uint32_t visibleObjects = 0;
        uint32_t culledObjects = 0;
        uint32_t lights = 0;
        uint32_t shadowCasters = 0;
        uint32_t localShadowLights = 0;
        uint32_t localShadowViews = 0;
    };
}
