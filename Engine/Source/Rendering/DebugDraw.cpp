#include "Rendering/DebugDraw.h"

#include <algorithm>

namespace BrightBox3D::Rendering
{
    namespace
    {
        struct TimedLine
        {
            LineVertex from;
            LineVertex to;
            float remaining = 0.0f;
            DepthTest depthTest = DepthTest::On;
        };

        std::vector<TimedLine>& TimedLines()
        {
            static std::vector<TimedLine> lines;
            return lines;
        }
    }

    std::vector<LineVertex>& DebugDraw::DepthTested()
    {
        static std::vector<LineVertex> lines;
        return lines;
    }

    std::vector<LineVertex>& DebugDraw::Overlay()
    {
        static std::vector<LineVertex> lines;
        return lines;
    }

    void DebugDraw::Line(const Math::Vector3& from, const Math::Vector3& to, const Math::Color& color, float duration, DepthTest depthTest)
    {
        uint32_t packed = color.ToRgba8();
        if (duration > 0.0f)
        {
            TimedLines().push_back({ { from, packed }, { to, packed }, duration, depthTest });
            return;
        }
        auto& target = depthTest == DepthTest::On ? DepthTested() : Overlay();
        target.push_back({ from, packed });
        target.push_back({ to, packed });
    }

    void DebugDraw::Ray(const Math::Vector3& origin, const Math::Vector3& direction, const Math::Color& color, float duration)
    {
        Line(origin, origin + direction, color, duration);
    }

    void DebugDraw::Box(const Math::Vector3& center, const Math::Vector3& extents, const Math::Color& color, float duration)
    {
        Math::Vector3 corners[8];
        for (int i = 0; i < 8; ++i)
        {
            corners[i] = center + Math::Vector3((i & 1) ? extents.x : -extents.x, (i & 2) ? extents.y : -extents.y, (i & 4) ? extents.z : -extents.z);
        }
        const int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
        for (const auto& edge : edges)
        {
            Line(corners[edge[0]], corners[edge[1]], color, duration);
        }
    }

    void DebugDraw::WireBox(const Math::Matrix4& transform, const Math::Vector3& extents, const Math::Color& color, float duration, DepthTest depthTest)
    {
        Math::Vector3 corners[8];
        for (int i = 0; i < 8; ++i)
        {
            corners[i] = transform.TransformPoint({ (i & 1) ? extents.x : -extents.x, (i & 2) ? extents.y : -extents.y, (i & 4) ? extents.z : -extents.z });
        }
        const int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
        for (const auto& edge : edges)
        {
            Line(corners[edge[0]], corners[edge[1]], color, duration, depthTest);
        }
    }

    void DebugDraw::Circle(const Math::Vector3& center, const Math::Vector3& normal, float radius, const Math::Color& color, int segments, DepthTest depthTest)
    {
        Math::Vector3 axis = std::abs(normal.y) < 0.99f ? Math::Vector3::Up() : Math::Vector3::Right();
        Math::Vector3 u = Math::Vector3::Cross(axis, normal).Normalized();
        Math::Vector3 v = Math::Vector3::Cross(normal, u).Normalized();
        Math::Vector3 previous = center + u * radius;
        for (int i = 1; i <= segments; ++i)
        {
            float angle = static_cast<float>(i) / static_cast<float>(segments) * Math::TwoPi;
            Math::Vector3 point = center + (u * std::cos(angle) + v * std::sin(angle)) * radius;
            Line(previous, point, color, 0.0f, depthTest);
            previous = point;
        }
    }

    void DebugDraw::Sphere(const Math::Vector3& center, float radius, const Math::Color& color, float duration)
    {
        Circle(center, Math::Vector3::Up(), radius, color);
        Circle(center, Math::Vector3::Right(), radius, color);
        Circle(center, Math::Vector3::Forward(), radius, color);
    }

    void DebugDraw::Cone(const Math::Vector3& apex, const Math::Vector3& direction, float length, float angleDegrees, const Math::Color& color, DepthTest depthTest)
    {
        Math::Vector3 forward = direction.Normalized();
        float radius = std::tan(angleDegrees * Math::DegreesToRadians) * length;
        Math::Vector3 center = apex + forward * length;
        Circle(center, forward, radius, color, 32, depthTest);
        Math::Vector3 axis = std::abs(forward.y) < 0.99f ? Math::Vector3::Up() : Math::Vector3::Right();
        Math::Vector3 u = Math::Vector3::Cross(axis, forward).Normalized();
        Math::Vector3 v = Math::Vector3::Cross(forward, u).Normalized();
        Line(apex, center + u * radius, color, 0.0f, depthTest);
        Line(apex, center - u * radius, color, 0.0f, depthTest);
        Line(apex, center + v * radius, color, 0.0f, depthTest);
        Line(apex, center - v * radius, color, 0.0f, depthTest);
    }

    void DebugDraw::Frustum(const Math::Matrix4& inverseViewProjection, const Math::Color& color, DepthTest depthTest)
    {
        Math::Vector3 corners[8];
        for (int i = 0; i < 8; ++i)
        {
            corners[i] = inverseViewProjection.TransformPoint({ (i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : 0.0f });
        }
        const int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
        for (const auto& edge : edges)
        {
            Line(corners[edge[0]], corners[edge[1]], color, 0.0f, depthTest);
        }
    }

    void DebugDraw::Advance(float deltaTime)
    {
        auto& timed = TimedLines();
        for (TimedLine& line : timed)
        {
            auto& target = line.depthTest == DepthTest::On ? DepthTested() : Overlay();
            target.push_back(line.from);
            target.push_back(line.to);
            line.remaining -= deltaTime;
        }
        timed.erase(std::remove_if(timed.begin(), timed.end(), [](const TimedLine& line) { return line.remaining <= 0.0f; }), timed.end());
    }

    void DebugDraw::Clear()
    {
        DepthTested().clear();
        Overlay().clear();
    }
}
