#pragma once

#include "Math/Geometry.h"
#include "Math/Vector3.h"
#include "Rendering/Light.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace BrightBox3D::Rendering
{
    struct RenderView;

    constexpr uint32_t MinShadowTileSize = 128;

    struct LocalShadowCandidate
    {
        uint32_t lightIndex = 0;
        LightType type = LightType::Point;
        Math::Vector3 position;
        Math::Vector3 direction;
        float range = 0.0f;
        float outerAngle = 0.0f;
        float intensity = 0.0f;
        uint32_t maxTileSize = 0;
        float bias = 0.0f;
        float normalBias = 0.0f;
    };

    struct RankedLocalShadow
    {
        size_t candidate = 0;
        float score = 0.0f;
        uint32_t tileSize = 0;
    };
}

namespace BrightBox3D::Rendering::LocalShadowSelection
{
    float Score(const LocalShadowCandidate& candidate, const Math::Vector3& cameraPosition);
    uint32_t RequestedTileSize(const LocalShadowCandidate& candidate, const RenderView& view);
    std::vector<RankedLocalShadow> Rank(const std::vector<LocalShadowCandidate>& candidates, const RenderView& view, const Math::Frustum& frustum);
}
