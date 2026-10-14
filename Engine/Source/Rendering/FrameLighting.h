#pragma once

#include "Math/Vector3.h"

#include <cstdint>
#include <vector>

namespace BrightBox3D::RHI
{
    class CommandList;
    class Texture;
}

namespace BrightBox3D::Rendering
{
    class GpuResourceCache;
    struct ViewConstants;

    struct FrameLighting
    {
        FrameLighting();

        void Bind(RHI::CommandList& commands, const ViewConstants& viewConstants, GpuResourceCache& resources) const;

        std::vector<uint8_t> lightConstants;
        std::vector<uint8_t> shadowConstants;
        Math::Vector3 shadowDirection;
        bool hasShadowLight = false;
        uint32_t lightCount = 0;
        RHI::Texture* shadowMap = nullptr;
    };
}
