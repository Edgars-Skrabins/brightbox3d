#include "Math/Geometry.h"

namespace BrightBox3D::Math
{
    void Bounds::Encapsulate(const Vector3& point)
    {
        minimum = Vector3::Min(minimum, point);
        maximum = Vector3::Max(maximum, point);
    }

    void Bounds::Encapsulate(const Bounds& other)
    {
        if (!other.IsValid())
        {
            return;
        }
        Encapsulate(other.minimum);
        Encapsulate(other.maximum);
    }

    Bounds Bounds::Transformed(const Matrix4& matrix) const
    {
        Bounds result;
        if (!IsValid())
        {
            return result;
        }
        for (int i = 0; i < 8; ++i)
        {
            Vector3 corner((i & 1) ? maximum.x : minimum.x, (i & 2) ? maximum.y : minimum.y, (i & 4) ? maximum.z : minimum.z);
            result.Encapsulate(matrix.TransformPoint(corner));
        }
        return result;
    }

    bool Bounds::Contains(const Vector3& point) const
    {
        return point.x >= minimum.x && point.y >= minimum.y && point.z >= minimum.z && point.x <= maximum.x && point.y <= maximum.y && point.z <= maximum.z;
    }

    bool Ray::IntersectsBounds(const Bounds& bounds, float& distance) const
    {
        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();
        for (int axis = 0; axis < 3; ++axis)
        {
            float origin1 = origin[axis];
            float direction1 = direction[axis];
            if (std::abs(direction1) < Epsilon)
            {
                if (origin1 < bounds.minimum[axis] || origin1 > bounds.maximum[axis])
                {
                    return false;
                }
                continue;
            }
            float inverse = 1.0f / direction1;
            float t1 = (bounds.minimum[axis] - origin1) * inverse;
            float t2 = (bounds.maximum[axis] - origin1) * inverse;
            if (t1 > t2)
            {
                std::swap(t1, t2);
            }
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax)
            {
                return false;
            }
        }
        distance = tMin;
        return true;
    }

    bool Ray::IntersectsTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float& distance) const
    {
        Vector3 edge1 = b - a;
        Vector3 edge2 = c - a;
        Vector3 p = Vector3::Cross(direction, edge2);
        float determinant = Vector3::Dot(edge1, p);
        if (std::abs(determinant) < Epsilon)
        {
            return false;
        }
        float inverse = 1.0f / determinant;
        Vector3 t = origin - a;
        float u = Vector3::Dot(t, p) * inverse;
        if (u < 0.0f || u > 1.0f)
        {
            return false;
        }
        Vector3 q = Vector3::Cross(t, edge1);
        float v = Vector3::Dot(direction, q) * inverse;
        if (v < 0.0f || u + v > 1.0f)
        {
            return false;
        }
        float hit = Vector3::Dot(edge2, q) * inverse;
        if (hit < 0.0f)
        {
            return false;
        }
        distance = hit;
        return true;
    }

    bool Ray::IntersectsSphere(const Vector3& center, float radius, float& distance) const
    {
        Vector3 offset = origin - center;
        float b = Vector3::Dot(offset, direction);
        float c = Vector3::Dot(offset, offset) - radius * radius;
        if (c > 0.0f && b > 0.0f)
        {
            return false;
        }
        float discriminant = b * b - c;
        if (discriminant < 0.0f)
        {
            return false;
        }
        distance = std::max(0.0f, -b - std::sqrt(discriminant));
        return true;
    }

    Plane Plane::Normalized() const
    {
        float length = normal.Length();
        if (length < Epsilon)
        {
            return *this;
        }
        return { normal / length, distance / length };
    }

    bool Plane::Raycast(const Ray& ray, float& hitDistance) const
    {
        float denominator = Vector3::Dot(normal, ray.direction);
        if (std::abs(denominator) < Epsilon)
        {
            return false;
        }
        float t = -(Vector3::Dot(normal, ray.origin) + distance) / denominator;
        if (t < 0.0f)
        {
            return false;
        }
        hitDistance = t;
        return true;
    }

    Frustum Frustum::FromViewProjection(const Matrix4& viewProjection)
    {
        const auto& m = viewProjection.m;
        auto column = [&](int index)
        {
            return Vector4(m[0][index], m[1][index], m[2][index], m[3][index]);
        };
        Vector4 c0 = column(0);
        Vector4 c1 = column(1);
        Vector4 c2 = column(2);
        Vector4 c3 = column(3);
        Vector4 raw[6] = { c3 + c0, c3 - c0, c3 + c1, c3 - c1, c2, c3 - c2 };

        Frustum frustum;
        for (int i = 0; i < 6; ++i)
        {
            Plane plane{ raw[i].XYZ(), raw[i].w };
            frustum.planes[static_cast<size_t>(i)] = plane.Normalized();
        }
        return frustum;
    }

    bool Frustum::Intersects(const Bounds& bounds) const
    {
        Vector3 center = bounds.Center();
        Vector3 extents = bounds.Extents();
        for (const Plane& plane : planes)
        {
            float radius = extents.x * std::abs(plane.normal.x) + extents.y * std::abs(plane.normal.y) + extents.z * std::abs(plane.normal.z);
            if (plane.SignedDistance(center) < -radius)
            {
                return false;
            }
        }
        return true;
    }
}
