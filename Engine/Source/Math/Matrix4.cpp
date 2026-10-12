#include "Math/Matrix4.h"

namespace BrightBox3D::Math
{
    Matrix4 Matrix4::operator*(const Matrix4& other) const
    {
        Matrix4 result;
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                result.m[row][column] = m[row][0] * other.m[0][column] + m[row][1] * other.m[1][column] + m[row][2] * other.m[2][column] + m[row][3] * other.m[3][column];
            }
        }
        return result;
    }

    bool Matrix4::operator==(const Matrix4& other) const
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                if (m[row][column] != other.m[row][column])
                {
                    return false;
                }
            }
        }
        return true;
    }

    Vector4 Matrix4::Transform(const Vector4& vector) const
    {
        return {
            vector.x * m[0][0] + vector.y * m[1][0] + vector.z * m[2][0] + vector.w * m[3][0],
            vector.x * m[0][1] + vector.y * m[1][1] + vector.z * m[2][1] + vector.w * m[3][1],
            vector.x * m[0][2] + vector.y * m[1][2] + vector.z * m[2][2] + vector.w * m[3][2],
            vector.x * m[0][3] + vector.y * m[1][3] + vector.z * m[2][3] + vector.w * m[3][3]
        };
    }

    Vector3 Matrix4::TransformPoint(const Vector3& point) const
    {
        Vector4 result = Transform(Vector4(point, 1.0f));
        if (std::abs(result.w) > Epsilon && result.w != 1.0f)
        {
            return result.XYZ() / result.w;
        }
        return result.XYZ();
    }

    Vector3 Matrix4::TransformDirection(const Vector3& direction) const
    {
        return Transform(Vector4(direction, 0.0f)).XYZ();
    }

    Matrix4 Matrix4::Transposed() const
    {
        Matrix4 result;
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                result.m[row][column] = m[column][row];
            }
        }
        return result;
    }

    Matrix4 Matrix4::Inverse() const
    {
        const float* a = Data();
        float inverse[16];

        inverse[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
        inverse[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
        inverse[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
        inverse[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
        inverse[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
        inverse[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
        inverse[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
        inverse[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
        inverse[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
        inverse[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
        inverse[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
        inverse[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
        inverse[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
        inverse[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
        inverse[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
        inverse[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

        float determinant = a[0] * inverse[0] + a[1] * inverse[4] + a[2] * inverse[8] + a[3] * inverse[12];
        Matrix4 result;
        if (std::abs(determinant) < 1.0e-12f)
        {
            return result;
        }
        float inverseDeterminant = 1.0f / determinant;
        for (int i = 0; i < 16; ++i)
        {
            result.m[i / 4][i % 4] = inverse[i] * inverseDeterminant;
        }
        return result;
    }

    void Matrix4::Decompose(Vector3& translation, Quaternion& rotation, Vector3& scale) const
    {
        translation = GetTranslation();
        Vector3 right = GetRight();
        Vector3 up = GetUp();
        Vector3 forward = GetForward();
        scale = { right.Length(), up.Length(), forward.Length() };
        if (Vector3::Dot(Vector3::Cross(right, up), forward) < 0.0f)
        {
            scale.x = -scale.x;
            right = -right;
        }
        if (std::abs(scale.x) < Epsilon || std::abs(scale.y) < Epsilon || std::abs(scale.z) < Epsilon)
        {
            rotation = Quaternion::Identity();
            return;
        }
        rotation = Quaternion::FromBasis(right / std::abs(scale.x), up / scale.y, forward / scale.z);
    }

    Matrix4 Matrix4::Translation(const Vector3& translation)
    {
        Matrix4 result;
        result.m[3][0] = translation.x;
        result.m[3][1] = translation.y;
        result.m[3][2] = translation.z;
        return result;
    }

    Matrix4 Matrix4::Scale(const Vector3& scale)
    {
        Matrix4 result;
        result.m[0][0] = scale.x;
        result.m[1][1] = scale.y;
        result.m[2][2] = scale.z;
        return result;
    }

    Matrix4 Matrix4::Rotation(const Quaternion& rotation)
    {
        Quaternion q = rotation.Normalized();
        float xx = q.x * q.x;
        float yy = q.y * q.y;
        float zz = q.z * q.z;
        float xy = q.x * q.y;
        float xz = q.x * q.z;
        float yz = q.y * q.z;
        float wx = q.w * q.x;
        float wy = q.w * q.y;
        float wz = q.w * q.z;

        Matrix4 result;
        result.m[0][0] = 1.0f - 2.0f * (yy + zz);
        result.m[0][1] = 2.0f * (xy + wz);
        result.m[0][2] = 2.0f * (xz - wy);
        result.m[1][0] = 2.0f * (xy - wz);
        result.m[1][1] = 1.0f - 2.0f * (xx + zz);
        result.m[1][2] = 2.0f * (yz + wx);
        result.m[2][0] = 2.0f * (xz + wy);
        result.m[2][1] = 2.0f * (yz - wx);
        result.m[2][2] = 1.0f - 2.0f * (xx + yy);
        return result;
    }

    Matrix4 Matrix4::TRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
    {
        Matrix4 result = Rotation(rotation);
        for (int column = 0; column < 3; ++column)
        {
            result.m[0][column] *= scale.x;
            result.m[1][column] *= scale.y;
            result.m[2][column] *= scale.z;
        }
        result.m[3][0] = translation.x;
        result.m[3][1] = translation.y;
        result.m[3][2] = translation.z;
        return result;
    }

    Matrix4 Matrix4::LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up)
    {
        Vector3 zAxis = (target - eye).Normalized();
        Vector3 xAxis = Vector3::Cross(up, zAxis).Normalized();
        Vector3 yAxis = Vector3::Cross(zAxis, xAxis);

        Matrix4 result;
        result.m[0][0] = xAxis.x;
        result.m[0][1] = yAxis.x;
        result.m[0][2] = zAxis.x;
        result.m[1][0] = xAxis.y;
        result.m[1][1] = yAxis.y;
        result.m[1][2] = zAxis.y;
        result.m[2][0] = xAxis.z;
        result.m[2][1] = yAxis.z;
        result.m[2][2] = zAxis.z;
        result.m[3][0] = -Vector3::Dot(xAxis, eye);
        result.m[3][1] = -Vector3::Dot(yAxis, eye);
        result.m[3][2] = -Vector3::Dot(zAxis, eye);
        return result;
    }

    Matrix4 Matrix4::PerspectiveFovLH(float fovYRadians, float aspect, float nearPlane, float farPlane)
    {
        float yScale = 1.0f / std::tan(fovYRadians * 0.5f);
        float xScale = yScale / aspect;
        float range = farPlane / (farPlane - nearPlane);

        Matrix4 result;
        result.m[0][0] = xScale;
        result.m[1][1] = yScale;
        result.m[2][2] = range;
        result.m[2][3] = 1.0f;
        result.m[3][2] = -nearPlane * range;
        result.m[3][3] = 0.0f;
        return result;
    }

    Matrix4 Matrix4::OrthographicLH(float width, float height, float nearPlane, float farPlane)
    {
        return OrthographicOffCenterLH(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, nearPlane, farPlane);
    }

    Matrix4 Matrix4::OrthographicOffCenterLH(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    {
        Matrix4 result;
        result.m[0][0] = 2.0f / (right - left);
        result.m[1][1] = 2.0f / (top - bottom);
        result.m[2][2] = 1.0f / (farPlane - nearPlane);
        result.m[3][0] = (left + right) / (left - right);
        result.m[3][1] = (top + bottom) / (bottom - top);
        result.m[3][2] = nearPlane / (nearPlane - farPlane);
        return result;
    }
}
