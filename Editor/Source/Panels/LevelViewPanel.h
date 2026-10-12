#pragma once

#include "EditorCamera.h"
#include "Math/Geometry.h"
#include "RHI/Resources.h"
#include "Rendering/RenderView.h"
#include "Serialization/Json.h"

#include <imgui.h>

#include <memory>

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class LevelViewPanel
    {
    public:
        explicit LevelViewPanel(EditorApplication& application);
        ~LevelViewPanel();

        void Draw(float deltaTime);
        EditorCamera& GetCamera() { return m_camera; }
        void FocusSelection();
        Math::Vector3 GetSpawnPoint() const;

    private:
        void DrawToolbar();
        void DrawGizmo(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size);
        void DrawIcons(const Math::Matrix4& viewProjection, const ImVec2& origin, const ImVec2& size);
        void DrawSelectionOverlay();
        void HandlePicking(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size);
        void HandleDrop(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size);
        Math::Ray ScreenRay(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size, const ImVec2& mouse) const;
        Math::Vector3 PlacementPoint(const Math::Ray& ray) const;

        EditorApplication& m_application;
        EditorCamera m_camera;
        std::unique_ptr<RHI::Texture> m_target;
        Rendering::RenderDebugMode m_debugMode = Rendering::RenderDebugMode::Lit;
        bool m_showGrid = true;
        bool m_gizmoActive = false;
        bool m_iconHovered = false;
        Serialization::JsonDocument m_gizmoBefore;
        Core::Guid m_gizmoObject;
        Math::Vector3 m_lastSpawnPoint;
    };
}
