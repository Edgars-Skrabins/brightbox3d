#pragma once

#include "Math/Matrix4.h"

#include <array>
#include <limits>

namespace BrightBox3D::Math
{
    struct Bounds
    {
        Vector3 minimum{ std::numeric_limits<float>::max() };
        Vector3 maximum{ -std::numeric_limits<float>::max() };

        bool IsValid() const { return minimum.x <= maximum.x && minimum.y <= maximum.y && minimum.z <= maximum.z; }
        Vector3 Center() const { return (minimum + maximum) * 0.5f; }
        Vector3 Extents() const { return (maximum - minimum) * 0.5f; }
        Vector3 Size() const { return maximum - minimum; }
        void Encapsulate(const Vector3& point);
        void Encapsulate(const Bounds& other);
        Bounds Transformed(const Matrix4& matrix) const;
        bool Contains(const Vector3& point) const;
    };

    struct Ray
    {
        Vector3 origin;
        Vector3 direction{ 0.0f, 0.0f, 1.0f };

        Vector3 GetPoint(float distance) const { return origin + direction * distance; }
        bool IntersectsBounds(const Bounds& bounds, float& distance) const;
        bool IntersectsTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float& distance) const;
        bool IntersectsSphere(const Vector3& center, float radius, float& distance) const;
    };

    struct Plane
    {
        Vector3 normal{ 0.0f, 1.0f, 0.0f };
        float distance = 0.0f;

        Plane Normalized() const;
        float SignedDistance(const Vector3& point) const { return Vector3::Dot(normal, point) + distance; }
        bool Raycast(const Ray& ray, float& hitDistance) const;
    };

    struct Frustum
    {
        std::array<Plane, 6> planes;

        static Frustum FromViewProjection(const Matrix4& viewProjection);
        bool Intersects(const Bounds& bounds) const;
    };
}
