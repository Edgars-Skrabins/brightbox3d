#include "Math/MathUtils.h"
#include "Rendering/LocalShadowSelection.h"
#include "Rendering/RenderView.h"

#include <gtest/gtest.h>

using namespace BrightBox3D;
using namespace BrightBox3D::Rendering;

namespace
{
    RenderView MakeView()
    {
        RenderView view;
        view.position = Math::Vector3::Zero();
        view.verticalFov = 60.0f;
        view.view = Math::Matrix4::LookAtLH(view.position, Math::Vector3::Forward(), Math::Vector3::Up());
        view.projection = Math::Matrix4::PerspectiveFovLH(view.verticalFov * Math::DegreesToRadians, 1.0f, view.nearPlane, view.farPlane);
        view.shadows.localDistance = 40.0f;
        return view;
    }

    Math::Frustum MakeFrustum(const RenderView& view)
    {
        return Math::Frustum::FromViewProjection(view.view * view.projection);
    }

    LocalShadowCandidate MakeCandidate(uint32_t lightIndex, const Math::Vector3& position, float range, float intensity)
    {
        LocalShadowCandidate candidate;
        candidate.lightIndex = lightIndex;
        candidate.position = position;
        candidate.range = range;
        candidate.intensity = intensity;
        candidate.maxTileSize = 1024;
        return candidate;
    }
}

TEST(LocalShadowSelectionTests, ScoreFallsWithDistanceOutsideTheRange)
{
    LocalShadowCandidate candidate = MakeCandidate(0, { 0.0f, 0.0f, 10.0f }, 5.0f, 2.0f);
    EXPECT_FLOAT_EQ(LocalShadowSelection::Score(candidate, Math::Vector3::Zero()), 2.0f * 25.0f / 100.0f);
    EXPECT_FLOAT_EQ(LocalShadowSelection::Score(candidate, { 0.0f, 0.0f, 8.0f }), 2.0f);
}

TEST(LocalShadowSelectionTests, LightsOutsideTheViewAreSkipped)
{
    RenderView view = MakeView();
    std::vector<LocalShadowCandidate> candidates = {
        MakeCandidate(0, { 0.0f, 0.0f, -20.0f }, 5.0f, 1.0f),
        MakeCandidate(1, { 0.0f, 0.0f, 10.0f }, 5.0f, 1.0f),
    };
    std::vector<RankedLocalShadow> ranked = LocalShadowSelection::Rank(candidates, view, MakeFrustum(view));
    ASSERT_EQ(ranked.size(), 1u);
    EXPECT_EQ(ranked[0].candidate, 1u);
}

TEST(LocalShadowSelectionTests, LightsBehindTheCameraStillCountWhenTheirRangeReachesTheView)
{
    RenderView view = MakeView();
    std::vector<LocalShadowCandidate> candidates = { MakeCandidate(0, { 0.0f, 0.0f, -2.0f }, 5.0f, 1.0f) };
    EXPECT_EQ(LocalShadowSelection::Rank(candidates, view, MakeFrustum(view)).size(), 1u);
}

TEST(LocalShadowSelectionTests, LightsBeyondTheLocalShadowDistanceAreSkipped)
{
    RenderView view = MakeView();
    std::vector<LocalShadowCandidate> candidates = {
        MakeCandidate(0, { 0.0f, 0.0f, 50.0f }, 5.0f, 1.0f),
        MakeCandidate(1, { 0.0f, 0.0f, 44.0f }, 5.0f, 1.0f),
    };
    std::vector<RankedLocalShadow> ranked = LocalShadowSelection::Rank(candidates, view, MakeFrustum(view));
    ASSERT_EQ(ranked.size(), 1u);
    EXPECT_EQ(ranked[0].candidate, 1u);
}

TEST(LocalShadowSelectionTests, RankingFollowsTheScore)
{
    RenderView view = MakeView();
    std::vector<LocalShadowCandidate> candidates = {
        MakeCandidate(0, { 0.0f, 0.0f, 20.0f }, 4.0f, 1.0f),
        MakeCandidate(1, { 0.0f, 0.0f, 10.0f }, 4.0f, 1.0f),
        MakeCandidate(2, { 0.0f, 0.0f, 20.0f }, 4.0f, 8.0f),
    };
    std::vector<RankedLocalShadow> ranked = LocalShadowSelection::Rank(candidates, view, MakeFrustum(view));
    ASSERT_EQ(ranked.size(), 3u);
    EXPECT_EQ(ranked[0].candidate, 2u);
    EXPECT_EQ(ranked[1].candidate, 1u);
    EXPECT_EQ(ranked[2].candidate, 0u);
    EXPECT_GE(ranked[0].score, ranked[1].score);
    EXPECT_GE(ranked[1].score, ranked[2].score);
}

TEST(LocalShadowSelectionTests, EqualScoresKeepTheLightOrder)
{
    RenderView view = MakeView();
    std::vector<LocalShadowCandidate> candidates = {
        MakeCandidate(5, { -2.0f, 0.0f, 10.0f }, 4.0f, 1.0f),
        MakeCandidate(2, { 2.0f, 0.0f, 10.0f }, 4.0f, 1.0f),
    };
    std::vector<RankedLocalShadow> ranked = LocalShadowSelection::Rank(candidates, view, MakeFrustum(view));
    ASSERT_EQ(ranked.size(), 2u);
    EXPECT_EQ(candidates[ranked[0].candidate].lightIndex, 2u);
    EXPECT_EQ(candidates[ranked[1].candidate].lightIndex, 5u);
}

TEST(LocalShadowSelectionTests, TileSizeHalvesAsScreenCoverageShrinks)
{
    RenderView view = MakeView();
    EXPECT_EQ(LocalShadowSelection::RequestedTileSize(MakeCandidate(0, { 0.0f, 0.0f, 2.0f }, 5.0f, 1.0f), view), 1024u);
    EXPECT_EQ(LocalShadowSelection::RequestedTileSize(MakeCandidate(0, { 0.0f, 0.0f, 10.0f }, 4.0f, 1.0f), view), 1024u);
    EXPECT_EQ(LocalShadowSelection::RequestedTileSize(MakeCandidate(0, { 0.0f, 0.0f, 10.0f }, 2.0f, 1.0f), view), 512u);
    EXPECT_EQ(LocalShadowSelection::RequestedTileSize(MakeCandidate(0, { 0.0f, 0.0f, 20.0f }, 1.0f, 1.0f), view), MinShadowTileSize);
}

TEST(LocalShadowSelectionTests, TileSizeNeverGoesBelowTheMinimum)
{
    RenderView view = MakeView();
    LocalShadowCandidate candidate = MakeCandidate(0, { 0.0f, 0.0f, 39.0f }, 0.1f, 1.0f);
    candidate.maxTileSize = 512;
    EXPECT_EQ(LocalShadowSelection::RequestedTileSize(candidate, view), MinShadowTileSize);
}
