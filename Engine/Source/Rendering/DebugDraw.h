#pragma once

#include "Math/Color.h"
#include "Math/Geometry.h"

#include <cstdint>
#include <vector>

namespace BrightBox3D::Rendering
{
    struct LineVertex
    {
        Math::Vector3 position;
        uint32_t color = 0xFFFFFFFF;
    };

    class DebugDraw
    {
    public:
        static void Line(const Math::Vector3& from, const Math::Vector3& to, const Math::Color& color = Math::Color::White(), float duration = 0.0f, bool depthTest = true);
        static void Ray(const Math::Vector3& origin, const Math::Vector3& direction, const Math::Color& color = Math::Color::White(), float duration = 0.0f);
        static void Box(const Math::Vector3& center, const Math::Vector3& extents, const Math::Color& color = Math::Color::White(), float duration = 0.0f);
        static void WireBox(const Math::Matrix4& transform, const Math::Vector3& extents, const Math::Color& color, float duration = 0.0f, bool depthTest = true);
        static void Sphere(const Math::Vector3& center, float radius, const Math::Color& color = Math::Color::White(), float duration = 0.0f);
        static void Circle(const Math::Vector3& center, const Math::Vector3& normal, float radius, const Math::Color& color, int segments = 32, bool depthTest = true);
        static void Cone(const Math::Vector3& apex, const Math::Vector3& direction, float length, float angleDegrees, const Math::Color& color, bool depthTest = true);
        static void Frustum(const Math::Matrix4& inverseViewProjection, const Math::Color& color, bool depthTest = true);

        static const std::vector<LineVertex>& GetDepthTested() { return DepthTested(); }
        static const std::vector<LineVertex>& GetOverlay() { return Overlay(); }
        static void Advance(float deltaTime);
        static void Clear();

    private:
        static std::vector<LineVertex>& DepthTested();
        static std::vector<LineVertex>& Overlay();
    };
}
