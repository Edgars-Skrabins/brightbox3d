#include "Rendering/LocalShadowSelection.h"

#include "Math/MathUtils.h"
#include "Rendering/RenderView.h"

#include <algorithm>
#include <cmath>

namespace BrightBox3D::Rendering::LocalShadowSelection
{
    namespace
    {
        constexpr float FullCoverage = 1.0f;
        constexpr float HalvingCoverage = 0.5f;

        float ScreenCoverage(const LocalShadowCandidate& candidate, const RenderView& view)
        {
            float distanceSquared = (candidate.position - view.position).LengthSquared();
            float rangeSquared = candidate.range * candidate.range;
            if (distanceSquared <= rangeSquared)
            {
                return FullCoverage;
            }
            float tangent = candidate.range / std::sqrt(distanceSquared - rangeSquared);
            float halfFovTangent = std::tan(view.verticalFov * Math::DegreesToRadians * 0.5f);
            return std::min(tangent / halfFovTangent, FullCoverage);
        }

        bool IsRelevant(const LocalShadowCandidate& candidate, const RenderView& view, const Math::Frustum& frustum)
        {
            float distance = (candidate.position - view.position).Length();
            if (distance - candidate.range > view.shadows.localDistance)
            {
                return false;
            }
            Math::Bounds reach;
            reach.minimum = candidate.position - Math::Vector3(candidate.range);
            reach.maximum = candidate.position + Math::Vector3(candidate.range);
            return frustum.Intersects(reach);
        }
    }

    float Score(const LocalShadowCandidate& candidate, const Math::Vector3& cameraPosition)
    {
        float rangeSquared = candidate.range * candidate.range;
        float distanceSquared = (candidate.position - cameraPosition).LengthSquared();
        return candidate.intensity * rangeSquared / std::max(distanceSquared, rangeSquared);
    }

    uint32_t RequestedTileSize(const LocalShadowCandidate& candidate, const RenderView& view)
    {
        uint32_t size = std::max(candidate.maxTileSize, MinShadowTileSize);
        float coverage = ScreenCoverage(candidate, view);
        while (coverage <= HalvingCoverage && size / 2 >= MinShadowTileSize)
        {
            size /= 2;
            coverage *= 2.0f;
        }
        return size;
    }

    std::vector<RankedLocalShadow> Rank(const std::vector<LocalShadowCandidate>& candidates, const RenderView& view, const Math::Frustum& frustum)
    {
        std::vector<RankedLocalShadow> ranked;
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            const LocalShadowCandidate& candidate = candidates[i];
            if (IsRelevant(candidate, view, frustum))
            {
                ranked.push_back({ i, Score(candidate, view.position), RequestedTileSize(candidate, view) });
            }
        }
        std::sort(ranked.begin(), ranked.end(), [&candidates](const RankedLocalShadow& a, const RankedLocalShadow& b)
        {
            if (a.score != b.score)
            {
                return a.score > b.score;
            }
            return candidates[a.candidate].lightIndex < candidates[b.candidate].lightIndex;
        });
        return ranked;
    }
}
