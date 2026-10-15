#include "Panels/LevelView/SelectionOverlay.h"

#include "EditorPreferences.h"
#include "Framework/Camera.h"
#include "Geometry/ModelAsset.h"
#include "Physics/Colliders.h"
#include "Rendering/DebugDraw.h"
#include "Rendering/Light.h"
#include "Rendering/MeshRenderer.h"
#include "Services/SelectionService.h"

#include <algorithm>

namespace BrightBox3D::Editor::SelectionOverlay
{
    namespace
    {
        constexpr int LightRangeSegments = 48;
        constexpr int ColliderSegments = 32;
        constexpr int DirectionalSegments = 24;
        constexpr float DirectionalLength = 2.0f;
        constexpr float DirectionalRadius = 0.4f;
        constexpr uint32_t PreviewAspectWidth = 16;
        constexpr uint32_t PreviewAspectHeight = 9;
        constexpr float PreviewFarPlane = 3.0f;
        constexpr Rendering::DepthTest DepthTested = Rendering::DepthTest::Off;

        void DrawAxisCircles(const Math::Vector3& center, float radius, const Math::Color& color, int segments)
        {
            for (const Math::Vector3& axis : { Math::Vector3::Up(), Math::Vector3::Right(), Math::Vector3::Forward() })
            {
                Rendering::DebugDraw::Circle(center, axis, radius, color, segments, DepthTested);
            }
        }

        void DrawMeshBounds(Level::LevelObject& node)
        {
            auto* renderer = node.GetComponent<Rendering::MeshRenderer>();
            if (!renderer)
            {
                return;
            }
            auto model = renderer->GetModel().Load();
            if (const Geometry::MeshData* mesh = model ? model->GetMesh(renderer->GetMeshIndex()) : nullptr)
            {
                Math::Matrix4 box = Math::Matrix4::Translation(mesh->bounds.Center()) * node.GetTransform().GetWorldMatrix();
                Rendering::DebugDraw::WireBox(box, mesh->bounds.Extents(), EditorPreferences::Get().GetColor(EditorColor::Accent), 0.0f, DepthTested);
            }
        }

        void DrawLightShape(Level::LevelObject& node)
        {
            auto* light = node.GetComponent<Rendering::Light>();
            if (!light)
            {
                return;
            }
            const Level::Transform& transform = node.GetTransform();
            Math::Color color = EditorPreferences::Get().GetColor(EditorColor::LightGizmo);
            switch (light->GetLightType())
            {
            case Rendering::LightType::Point:
                DrawAxisCircles(transform.GetPosition(), light->GetRange(), color, LightRangeSegments);
                break;
            case Rendering::LightType::Spot:
                Rendering::DebugDraw::Cone(transform.GetPosition(), transform.GetForward(), light->GetRange(), light->GetOuterAngle(), color, DepthTested);
                break;
            case Rendering::LightType::Directional:
            {
                Math::Vector3 start = transform.GetPosition();
                Rendering::DebugDraw::Line(start, start + transform.GetForward() * DirectionalLength, color, 0.0f, DepthTested);
                Rendering::DebugDraw::Circle(start, transform.GetForward(), DirectionalRadius, color, DirectionalSegments, DepthTested);
                break;
            }
            }
        }

        void DrawCapsule(Level::LevelObject& node, const Physics::CapsuleCollider& capsule, const Math::Vector3& center, const Math::Vector3& scale, const Math::Color& color)
        {
            const Level::Transform& transform = node.GetTransform();
            float radius = capsule.GetRadius() * std::max(scale.x, scale.z);
            float half = std::max(capsule.GetHeight() * 0.5f * scale.y - radius, 0.0f);
            Math::Vector3 up = transform.GetUp();
            Rendering::DebugDraw::Circle(center + up * half, up, radius, color, ColliderSegments, DepthTested);
            Rendering::DebugDraw::Circle(center - up * half, up, radius, color, ColliderSegments, DepthTested);
            Math::Vector3 right = transform.GetRight() * radius;
            Math::Vector3 forward = transform.GetForward() * radius;
            for (const Math::Vector3& side : { right, -right, forward, -forward })
            {
                Rendering::DebugDraw::Line(center + up * half + side, center - up * half + side, color, 0.0f, DepthTested);
            }
        }

        void DrawColliderShape(Level::LevelObject& node, Physics::Collider& collider)
        {
            Math::Color color = EditorPreferences::Get().GetColor(collider.IsTrigger() ? EditorColor::Trigger : EditorColor::Collider);
            const Math::Matrix4& world = node.GetTransform().GetWorldMatrix();
            Math::Vector3 center = world.TransformPoint(collider.GetCenter());
            Math::Vector3 scale = node.GetTransform().GetLossyScale();
            float uniform = std::max(scale.x, std::max(scale.y, scale.z));
            if (auto* box = Reflection::Cast<Physics::BoxCollider>(&collider))
            {
                Rendering::DebugDraw::WireBox(Math::Matrix4::Translation(collider.GetCenter()) * world, box->GetSize() * 0.5f, color, 0.0f, DepthTested);
            }
            else if (auto* sphere = Reflection::Cast<Physics::SphereCollider>(&collider))
            {
                DrawAxisCircles(center, sphere->GetRadius() * uniform, color, ColliderSegments);
            }
            else if (auto* capsule = Reflection::Cast<Physics::CapsuleCollider>(&collider))
            {
                DrawCapsule(node, *capsule, center, scale, color);
            }
        }

        void DrawCameraFrustum(Level::LevelObject& node)
        {
            auto* camera = Reflection::Cast<Framework::Camera>(&node);
            if (!camera)
            {
                return;
            }
            Rendering::RenderView view = camera->BuildRenderView(PreviewAspectWidth, PreviewAspectHeight, 0);
            float aspect = static_cast<float>(PreviewAspectWidth) / static_cast<float>(PreviewAspectHeight);
            Math::Matrix4 limited = Math::Matrix4::PerspectiveFovLH(view.verticalFov * Math::DegreesToRadians, aspect, view.nearPlane, std::min(view.farPlane, PreviewFarPlane));
            Rendering::DebugDraw::Frustum((view.view * limited).Inverse(), EditorPreferences::Get().GetColor(EditorColor::CameraGizmo), DepthTested);
        }

        void DrawNode(Level::LevelObject& node)
        {
            DrawMeshBounds(node);
            DrawLightShape(node);
            for (Physics::Collider* collider : node.GetComponents<Physics::Collider>())
            {
                DrawColliderShape(node, *collider);
            }
            DrawCameraFrustum(node);
            for (Level::LevelObject* child : node.GetChildren())
            {
                DrawNode(*child);
            }
        }
    }

    void Draw(SelectionService& selection)
    {
        for (Level::LevelObject* object : selection.GetSelectedObjects())
        {
            DrawNode(*object);
        }
    }
}
