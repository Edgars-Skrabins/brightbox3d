#include "Math/Quaternion.h"

namespace BrightBox3D::Math
{
    namespace
    {
        float NormalizeAngle(float degrees)
        {
            float result = std::fmod(degrees, 360.0f);
            if (result < 0.0f)
            {
                result += 360.0f;
            }
            if (result >= 360.0f - 1.0e-4f)
            {
                result = 0.0f;
            }
            return result;
        }
    }

    Quaternion Quaternion::operator*(const Quaternion& other) const
    {
        return {
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w,
            w * other.w - x * other.x - y * other.y - z * other.z
        };
    }

    Vector3 Quaternion::operator*(const Vector3& vector) const
    {
        Vector3 axis(x, y, z);
        Vector3 t = Vector3::Cross(axis, vector) * 2.0f;
        return vector + t * w + Vector3::Cross(axis, t);
    }

    Quaternion Quaternion::Normalized() const
    {
        float length = std::sqrt(x * x + y * y + z * z + w * w);
        if (length < Epsilon)
        {
            return Identity();
        }
        return { x / length, y / length, z / length, w / length };
    }

    Quaternion Quaternion::Inverse() const
    {
        float lengthSquared = x * x + y * y + z * z + w * w;
        if (lengthSquared < Epsilon)
        {
            return Identity();
        }
        return { -x / lengthSquared, -y / lengthSquared, -z / lengthSquared, w / lengthSquared };
    }

    Vector3 Quaternion::ToEuler() const
    {
        Quaternion q = Normalized();
        float r00 = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
        float r02 = 2.0f * (q.x * q.z + q.w * q.y);
        float r10 = 2.0f * (q.x * q.y + q.w * q.z);
        float r11 = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
        float r12 = 2.0f * (q.y * q.z - q.w * q.x);
        float r20 = 2.0f * (q.x * q.z - q.w * q.y);
        float r22 = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);

        float sinX = Clamp(-r12, -1.0f, 1.0f);
        float angleX = std::asin(sinX);
        float angleY = 0.0f;
        float angleZ = 0.0f;
        if (std::abs(sinX) < 0.99999f)
        {
            angleY = std::atan2(r02, r22);
            angleZ = std::atan2(r10, r11);
        }
        else
        {
            angleY = std::atan2(-r20, r00);
        }
        return { NormalizeAngle(angleX * RadiansToDegrees), NormalizeAngle(angleY * RadiansToDegrees), NormalizeAngle(angleZ * RadiansToDegrees) };
    }

    Quaternion Quaternion::Euler(const Vector3& degrees)
    {
        return Euler(degrees.x, degrees.y, degrees.z);
    }

    Quaternion Quaternion::Euler(float xDegrees, float yDegrees, float zDegrees)
    {
        Quaternion qx = AngleAxis(xDegrees, Vector3::Right());
        Quaternion qy = AngleAxis(yDegrees, Vector3::Up());
        Quaternion qz = AngleAxis(zDegrees, Vector3::Forward());
        return (qy * qx * qz).Normalized();
    }

    Quaternion Quaternion::AngleAxis(float degrees, const Vector3& axis)
    {
        Vector3 normalized = axis.Normalized();
        float half = degrees * DegreesToRadians * 0.5f;
        float s = std::sin(half);
        return { normalized.x * s, normalized.y * s, normalized.z * s, std::cos(half) };
    }

    Quaternion Quaternion::FromBasis(const Vector3& right, const Vector3& up, const Vector3& forward)
    {
        float m00 = right.x;
        float m10 = right.y;
        float m20 = right.z;
        float m01 = up.x;
        float m11 = up.y;
        float m21 = up.z;
        float m02 = forward.x;
        float m12 = forward.y;
        float m22 = forward.z;

        Quaternion q;
        float trace = m00 + m11 + m22;
        if (trace > 0.0f)
        {
            float s = std::sqrt(trace + 1.0f) * 2.0f;
            q.w = 0.25f * s;
            q.x = (m21 - m12) / s;
            q.y = (m02 - m20) / s;
            q.z = (m10 - m01) / s;
        }
        else if (m00 > m11 && m00 > m22)
        {
            float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
            q.w = (m21 - m12) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        }
        else if (m11 > m22)
        {
            float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
            q.w = (m02 - m20) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        }
        else
        {
            float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
            q.w = (m10 - m01) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }
        return q.Normalized();
    }

    Quaternion Quaternion::LookRotation(const Vector3& forward, const Vector3& up)
    {
        Vector3 f = forward.Normalized();
        if (f.LengthSquared() < Epsilon)
        {
            return Identity();
        }
        Vector3 r = Vector3::Cross(up, f);
        if (r.LengthSquared() < Epsilon)
        {
            r = Vector3::Cross(std::abs(f.y) < 0.99f ? Vector3::Up() : Vector3::Forward(), f);
        }
        r = r.Normalized();
        Vector3 u = Vector3::Cross(f, r);
        return FromBasis(r, u, f);
    }

    Quaternion Quaternion::FromToRotation(const Vector3& from, const Vector3& to)
    {
        Vector3 a = from.Normalized();
        Vector3 b = to.Normalized();
        float dot = Vector3::Dot(a, b);
        if (dot >= 1.0f - Epsilon)
        {
            return Identity();
        }
        if (dot <= -1.0f + Epsilon)
        {
            Vector3 axis = Vector3::Cross(Vector3::Right(), a);
            if (axis.LengthSquared() < Epsilon)
            {
                axis = Vector3::Cross(Vector3::Up(), a);
            }
            return AngleAxis(180.0f, axis);
        }
        Vector3 axis = Vector3::Cross(a, b);
        Quaternion q(axis.x, axis.y, axis.z, 1.0f + dot);
        return q.Normalized();
    }

    Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t)
    {
        Quaternion end = b;
        float cosine = Dot(a, b);
        if (cosine < 0.0f)
        {
            cosine = -cosine;
            end = { -b.x, -b.y, -b.z, -b.w };
        }
        if (cosine > 0.9995f)
        {
            Quaternion result(a.x + (end.x - a.x) * t, a.y + (end.y - a.y) * t, a.z + (end.z - a.z) * t, a.w + (end.w - a.w) * t);
            return result.Normalized();
        }
        float angle = std::acos(cosine);
        float sine = std::sin(angle);
        float weightA = std::sin((1.0f - t) * angle) / sine;
        float weightB = std::sin(t * angle) / sine;
        return { a.x * weightA + end.x * weightB, a.y * weightA + end.y * weightB, a.z * weightA + end.z * weightB, a.w * weightA + end.w * weightB };
    }

    float Quaternion::Dot(const Quaternion& a, const Quaternion& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }

    float Quaternion::Angle(const Quaternion& a, const Quaternion& b)
    {
        float dot = std::min(std::abs(Dot(a.Normalized(), b.Normalized())), 1.0f);
        return std::acos(dot) * 2.0f * RadiansToDegrees;
    }
}
