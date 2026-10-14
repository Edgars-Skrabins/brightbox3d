#include "Math/ColorSpace.h"
#include "Math/Geometry.h"
#include "Math/Matrix4.h"
#include "Math/Quaternion.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace BrightBox3D::Math;

namespace
{
    void ExpectVectorNear(const Vector3& actual, const Vector3& expected, float tolerance = 1.0e-4f)
    {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }
}

TEST(Vector3Tests, CrossFollowsLeftHandedAxes)
{
    ExpectVectorNear(Vector3::Cross(Vector3::Up(), Vector3::Forward()), Vector3::Right());
    ExpectVectorNear(Vector3::Cross(Vector3::Forward(), Vector3::Right()), Vector3::Up());
}

TEST(QuaternionTests, YawNinetyTurnsForwardToRight)
{
    ExpectVectorNear(Quaternion::Euler(0.0f, 90.0f, 0.0f) * Vector3::Forward(), Vector3::Right());
}

TEST(QuaternionTests, PitchNinetyTurnsForwardDown)
{
    ExpectVectorNear(Quaternion::Euler(90.0f, 0.0f, 0.0f) * Vector3::Forward(), Vector3::Down());
}

TEST(QuaternionTests, EulerRoundTrip)
{
    Vector3 angles(30.0f, 45.0f, 60.0f);
    ExpectVectorNear(Quaternion::Euler(angles).ToEuler(), angles, 1.0e-3f);
    Vector3 other(350.0f, 10.0f, 200.0f);
    Quaternion q = Quaternion::Euler(other);
    Quaternion back = Quaternion::Euler(q.ToEuler());
    EXPECT_NEAR(std::abs(Quaternion::Dot(q, back)), 1.0f, 1.0e-4f);
}

TEST(QuaternionTests, LookRotationMatchesDirection)
{
    Vector3 direction = Vector3(1.0f, 0.5f, -2.0f).Normalized();
    ExpectVectorNear(Quaternion::LookRotation(direction) * Vector3::Forward(), direction);
}

TEST(QuaternionTests, FromToRotation)
{
    Quaternion q = Quaternion::FromToRotation(Vector3::Right(), Vector3::Up());
    ExpectVectorNear(q * Vector3::Right(), Vector3::Up());
}

TEST(Matrix4Tests, RotationMatchesQuaternion)
{
    Quaternion q = Quaternion::Euler(20.0f, 70.0f, -30.0f);
    Vector3 v(0.3f, -1.2f, 2.0f);
    ExpectVectorNear(Matrix4::Rotation(q).TransformDirection(v), q * v);
}

TEST(Matrix4Tests, TRSAppliesScaleRotationTranslation)
{
    Quaternion q = Quaternion::Euler(0.0f, 90.0f, 0.0f);
    Matrix4 m = Matrix4::TRS({ 1.0f, 2.0f, 3.0f }, q, { 2.0f, 2.0f, 2.0f });
    ExpectVectorNear(m.TransformPoint({ 0.0f, 0.0f, 1.0f }), { 3.0f, 2.0f, 3.0f });
}

TEST(Matrix4Tests, InverseAndDecompose)
{
    Quaternion q = Quaternion::Euler(10.0f, 20.0f, 30.0f);
    Matrix4 m = Matrix4::TRS({ 4.0f, -2.0f, 7.0f }, q, { 1.5f, 2.0f, 0.5f });
    Matrix4 identity = m * m.Inverse();
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            EXPECT_NEAR(identity.m[row][column], row == column ? 1.0f : 0.0f, 1.0e-4f);
        }
    }
    Vector3 translation;
    Quaternion rotation;
    Vector3 scale;
    m.Decompose(translation, rotation, scale);
    ExpectVectorNear(translation, { 4.0f, -2.0f, 7.0f });
    ExpectVectorNear(scale, { 1.5f, 2.0f, 0.5f });
    EXPECT_NEAR(std::abs(Quaternion::Dot(rotation, q)), 1.0f, 1.0e-4f);
}

TEST(Matrix4Tests, PerspectiveMapsDepthToZeroOne)
{
    Matrix4 projection = Matrix4::PerspectiveFovLH(1.0f, 1.0f, 0.1f, 100.0f);
    EXPECT_NEAR(projection.TransformPoint({ 0.0f, 0.0f, 0.1f }).z, 0.0f, 1.0e-5f);
    EXPECT_NEAR(projection.TransformPoint({ 0.0f, 0.0f, 100.0f }).z, 1.0f, 1.0e-5f);
}

TEST(Matrix4Tests, LookAtPutsTargetOnPositiveZ)
{
    Matrix4 view = Matrix4::LookAtLH({ 0.0f, 0.0f, -5.0f }, Vector3::Zero(), Vector3::Up());
    ExpectVectorNear(view.TransformPoint(Vector3::Zero()), { 0.0f, 0.0f, 5.0f });
}

TEST(GeometryTests, FrustumCullsBehindCamera)
{
    Matrix4 viewProjection = Matrix4::LookAtLH({ 0.0f, 0.0f, -5.0f }, Vector3::Zero(), Vector3::Up()) * Matrix4::PerspectiveFovLH(1.0f, 1.0f, 0.1f, 100.0f);
    Frustum frustum = Frustum::FromViewProjection(viewProjection);
    Bounds visible{ { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f } };
    Bounds behind{ { -1.0f, -1.0f, -20.0f }, { 1.0f, 1.0f, -18.0f } };
    EXPECT_TRUE(frustum.Intersects(visible));
    EXPECT_FALSE(frustum.Intersects(behind));
}

TEST(GeometryTests, RayHitsBounds)
{
    Ray ray{ { 0.0f, 0.0f, -10.0f }, { 0.0f, 0.0f, 1.0f } };
    float distance = 0.0f;
    EXPECT_TRUE(ray.IntersectsBounds({ { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f } }, distance));
    EXPECT_NEAR(distance, 9.0f, 1.0e-4f);
}

TEST(ColorSpaceTests, MatchesKnownSrgbValues)
{
    EXPECT_FLOAT_EQ(SrgbToLinear(0.0f), 0.0f);
    EXPECT_NEAR(SrgbToLinear(1.0f), 1.0f, 1.0e-6f);
    EXPECT_NEAR(SrgbToLinear(0.5f), 0.214041f, 1.0e-5f);
    EXPECT_NEAR(LinearToSrgb(0.5f), 0.735357f, 1.0e-5f);
    EXPECT_NEAR(LinearToSrgb(0.0018f), 0.0018f * 12.92f, 1.0e-7f);
}

TEST(ColorSpaceTests, RoundTripsAndStaysContinuousAndIncreasing)
{
    float previous = -1.0f;
    for (int step = 0; step <= 1000; ++step)
    {
        float value = step / 1000.0f;
        float linear = SrgbToLinear(value);
        EXPECT_NEAR(LinearToSrgb(linear), value, 1.0e-5f) << value;
        EXPECT_GT(linear, previous) << value;
        previous = linear;
    }
    EXPECT_NEAR(SrgbLinearThreshold / SrgbLinearSlope, std::pow((SrgbLinearThreshold + SrgbOffset) / SrgbScale, SrgbGamma), 1.0e-6f);
    EXPECT_NEAR(LinearSrgbThreshold * SrgbLinearSlope, SrgbScale * std::pow(LinearSrgbThreshold, 1.0f / SrgbGamma) - SrgbOffset, 1.0e-5f);
}
