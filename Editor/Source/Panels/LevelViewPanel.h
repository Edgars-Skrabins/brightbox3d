#pragma once

#include "Core/Guid.h"
#include "EditorCamera.h"
#include "Math/Geometry.h"
#include "Panels/EditorPanel.h"
#include "Rendering/RenderView.h"
#include "RHI/Resources.h"
#include "Serialization/Json.h"

#include <imgui.h>

#include <memory>

namespace BrightBox3D::Editor
{
    class EditorApplication;

    enum class GizmoOperation
    {
        Translate,
        Rotate,
        Scale
    };

    class LevelViewPanel : public EditorPanel
    {
    public:
        LevelViewPanel(EditorApplication& application, int instanceIndex);
        ~LevelViewPanel();

        void Draw() override;
        EditorCamera& GetCamera() { return m_camera; }
        void FocusSelection();
        Math::Vector3 GetSpawnPoint() const;

        GizmoOperation GetGizmoOperation() const { return m_gizmoOperation; }
        void SetGizmoOperation(GizmoOperation operation) { m_gizmoOperation = operation; }
        bool IsGizmoLocal() const { return m_gizmoLocal; }
        void SetGizmoLocal(bool local) { m_gizmoLocal = local; }
        bool IsSnapping() const { return m_snapping; }
        void SetSnapping(bool snapping) { m_snapping = snapping; }
        bool IsGridVisible() const { return m_showGrid; }
        void SetGridVisible(bool visible) { m_showGrid = visible; }

    private:
        void DrawToolbar();
        void DrawLevelObjectEditorBar();
        void EnsureTarget(uint32_t width, uint32_t height);
        void UpdateCamera(float deltaTime, bool hovered);
        void SubmitRender(const Math::Matrix4& view, const Math::Matrix4& projection, uint32_t width, uint32_t height);
        void DrawGizmo(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size);
        void DrawIcons(const Math::Matrix4& viewProjection, const ImVec2& origin, const ImVec2& size);
        void HandlePicking(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size);
        Math::Ray ScreenRay(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size, const ImVec2& mouse) const;

        EditorApplication& m_application;
        EditorCamera m_camera;
        std::unique_ptr<RHI::Texture> m_target;
        Rendering::RenderDebugMode m_debugMode = Rendering::RenderDebugMode::Lit;
        bool m_showGrid = true;
        GizmoOperation m_gizmoOperation = GizmoOperation::Translate;
        bool m_gizmoLocal = false;
        bool m_snapping = false;
        bool m_gizmoActive = false;
        bool m_iconHovered = false;
        Serialization::JsonDocument m_gizmoBefore;
        Core::Guid m_gizmoObject;
        Math::Vector3 m_lastSpawnPoint;
    };
}
