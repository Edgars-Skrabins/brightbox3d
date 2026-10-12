#pragma once

#include "Math/Vector3.h"

namespace BrightBox3D::Math
{
    struct Quaternion
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;

        constexpr Quaternion() = default;
        constexpr Quaternion(float xValue, float yValue, float zValue, float wValue) : x(xValue), y(yValue), z(zValue), w(wValue) {}

        Quaternion operator*(const Quaternion& other) const;
        Vector3 operator*(const Vector3& vector) const;
        constexpr bool operator==(const Quaternion& other) const { return x == other.x && y == other.y && z == other.z && w == other.w; }
        constexpr bool operator!=(const Quaternion& other) const { return !(*this == other); }

        Quaternion Normalized() const;
        Quaternion Inverse() const;
        Vector3 ToEuler() const;

        static constexpr Quaternion Identity() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
        static Quaternion Euler(const Vector3& degrees);
        static Quaternion Euler(float xDegrees, float yDegrees, float zDegrees);
        static Quaternion AngleAxis(float degrees, const Vector3& axis);
        static Quaternion LookRotation(const Vector3& forward, const Vector3& up = Vector3::Up());
        static Quaternion FromToRotation(const Vector3& from, const Vector3& to);
        static Quaternion FromBasis(const Vector3& right, const Vector3& up, const Vector3& forward);
        static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
        static float Dot(const Quaternion& a, const Quaternion& b);
        static float Angle(const Quaternion& a, const Quaternion& b);
    };
}
