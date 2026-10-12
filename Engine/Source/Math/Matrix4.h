#pragma once

#include "Math/Quaternion.h"
#include "Math/Vector4.h"

namespace BrightBox3D::Math
{
    struct Matrix4
    {
        float m[4][4] = {
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f }
        };

        Matrix4 operator*(const Matrix4& other) const;
        bool operator==(const Matrix4& other) const;
        bool operator!=(const Matrix4& other) const { return !(*this == other); }

        Vector4 Transform(const Vector4& vector) const;
        Vector3 TransformPoint(const Vector3& point) const;
        Vector3 TransformDirection(const Vector3& direction) const;
        Matrix4 Transposed() const;
        Matrix4 Inverse() const;
        Vector3 GetTranslation() const { return { m[3][0], m[3][1], m[3][2] }; }
        Vector3 GetRight() const { return { m[0][0], m[0][1], m[0][2] }; }
        Vector3 GetUp() const { return { m[1][0], m[1][1], m[1][2] }; }
        Vector3 GetForward() const { return { m[2][0], m[2][1], m[2][2] }; }
        void Decompose(Vector3& translation, Quaternion& rotation, Vector3& scale) const;
        const float* Data() const { return &m[0][0]; }

        static Matrix4 Identity() { return Matrix4(); }
        static Matrix4 Translation(const Vector3& translation);
        static Matrix4 Scale(const Vector3& scale);
        static Matrix4 Rotation(const Quaternion& rotation);
        static Matrix4 TRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale);
        static Matrix4 LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up);
        static Matrix4 PerspectiveFovLH(float fovYRadians, float aspect, float nearPlane, float farPlane);
        static Matrix4 OrthographicLH(float width, float height, float nearPlane, float farPlane);
        static Matrix4 OrthographicOffCenterLH(float left, float right, float bottom, float top, float nearPlane, float farPlane);
    };
}
