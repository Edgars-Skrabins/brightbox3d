#include "Panels/LevelViewPanel.h"

#include "Commands/CommandRegistry.h"
#include "Commands/CommandWidgets.h"
#include "EditorApplication.h"
#include "EditorPlaySession.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "EditorWindows.h"
#include "Framework/Camera.h"
#include "Framework/PlayerStart.h"
#include "Geometry/ModelAsset.h"
#include "IconGlyphs.h"
#include "ImGui/ImGuiRenderer.h"
#include "Level/LevelSerializer.h"
#include "Panels/LevelView/LevelViewDropTarget.h"
#include "Panels/LevelView/PlayModeOverlay.h"
#include "Panels/LevelView/SelectionOverlay.h"
#include "Picking.h"
#include "Rendering/Light.h"
#include "Rendering/MeshRenderer.h"

#include <ImGuizmo.h>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr uint64_t LevelViewId = 0x4C455645;
        constexpr float SpawnDistance = 6.0f;
        constexpr float DefaultFocusRadius = 1.0f;
        constexpr float DebugModeComboWidth = 110.0f;
        constexpr float SpeedFieldWidth = 90.0f;
        constexpr float SpeedDragStep = 0.1f;
        constexpr float MinCameraSpeed = 0.5f;
        constexpr float MaxCameraSpeed = 500.0f;
        constexpr const char* SpeedValueFormat = " %.1f";
        constexpr float IconRadius = 12.0f;
        constexpr float IconMinClipW = 0.1f;
        constexpr float IconOutline = 1.5f;
        constexpr float IconSelectedOutline = 2.5f;
        constexpr float IconGlyphOffsetY = 1.0f;
        constexpr float ToolbarOffsetX = 6.0f;
        constexpr float ToolbarOffsetY = 4.0f;
        constexpr float MinViewportSize = 8.0f;

        struct GizmoMode
        {
            ImGuizmo::OPERATION operation;
            float snap;
        };

        constexpr GizmoMode GizmoModes[] = {
            { ImGuizmo::TRANSLATE, 0.5f },
            { ImGuizmo::ROTATE, 15.0f },
            { ImGuizmo::SCALE, 0.1f }
        };

        const ToolbarItem LevelViewToolbar[] = {
            CommandId::GizmoMove, CommandId::GizmoRotate, CommandId::GizmoScale, ToolbarSeparator,
            CommandId::GizmoSpace, CommandId::Snap, CommandId::Grid,
        };

        void GatherMeshBounds(Level::LevelObject& node, Math::Bounds& bounds)
        {
            if (auto* renderer = node.GetComponent<Rendering::MeshRenderer>())
            {
                auto model = renderer->GetModel().Load();
                if (const Geometry::MeshData* mesh = model ? model->GetMesh(renderer->GetMeshIndex()) : nullptr)
                {
                    bounds.Encapsulate(mesh->bounds.Transformed(node.GetTransform().GetWorldMatrix()));
                }
            }
            for (Level::LevelObject* child : node.GetChildren())
            {
                GatherMeshBounds(*child, bounds);
            }
        }

        struct ObjectIcon
        {
            const char* glyph = nullptr;
            ImU32 color = 0;
        };

        ObjectIcon IconFor(const Level::LevelObject& object)
        {
            if (const Rendering::Light* light = object.GetComponent<Rendering::Light>())
            {
                return { light->GetLightType() == Rendering::LightType::Directional ? IconGlyph(Icon::Sun) : IconGlyph(Icon::Lightbulb), EditorColorU32(EditorColor::LightGizmo) };
            }
            if (object.IsA<Framework::Camera>())
            {
                return { IconGlyph(Icon::Video), EditorColorU32(EditorColor::CameraGizmo) };
            }
            if (object.IsA<Framework::PlayerStart>())
            {
                return { IconGlyph(Icon::Flag), EditorColorU32(EditorColor::PlayerStartGizmo) };
            }
            return {};
        }
    }

    LevelViewPanel::LevelViewPanel(EditorApplication& application, int instanceIndex) : EditorPanel(EditorWindow::LevelView, instanceIndex), m_application(application)
    {
    }

    LevelViewPanel::~LevelViewPanel()
    {
        m_application.GetDevice().WaitIdle();
        m_application.GetRenderer().ReleaseView(LevelViewId);
    }

    Math::Vector3 LevelViewPanel::GetSpawnPoint() const
    {
        return m_camera.GetPosition() + m_camera.GetForward() * SpawnDistance;
    }

    void LevelViewPanel::FocusSelection()
    {
        Level::LevelObject* object = m_application.GetContext().GetSelectionService().GetPrimarySelection();
        if (!object)
        {
            return;
        }
        Math::Bounds bounds;
        GatherMeshBounds(*object, bounds);
        if (bounds.IsValid())
        {
            m_camera.Focus(bounds.Center(), bounds.Extents().Length());
        }
        else
        {
            m_camera.Focus(object->GetTransform().GetPosition(), DefaultFocusRadius);
        }
    }

    Math::Ray LevelViewPanel::ScreenRay(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size, const ImVec2& mouse) const
    {
        float x = (mouse.x - origin.x) / size.x * 2.0f - 1.0f;
        float y = 1.0f - (mouse.y - origin.y) / size.y * 2.0f;
        Math::Matrix4 inverse = (view * projection).Inverse();
        Math::Vector3 nearPoint = inverse.TransformPoint({ x, y, 0.0f });
        Math::Vector3 farPoint = inverse.TransformPoint({ x, y, 1.0f });
        return { nearPoint, (farPoint - nearPoint).Normalized() };
    }

    void LevelViewPanel::DrawToolbar()
    {
        Editor::DrawToolbar(LevelViewToolbar);
        ImGui::SameLine();
        static const char* modes[] = { Text(EditorText::DebugModeLit), Text(EditorText::DebugModeAlbedo), Text(EditorText::DebugModeNormals), Text(EditorText::DebugModeMetallicRoughness), Text(EditorText::DebugModeShadows), Text(EditorText::DebugModeAmbientOcclusion) };
        ImGui::SetNextItemWidth(DebugModeComboWidth);
        int mode = static_cast<int>(m_debugMode);
        if (ImGui::Combo("##debugmode", &mode, modes, IM_ARRAYSIZE(modes)))
        {
            m_debugMode = static_cast<Rendering::RenderDebugMode>(mode);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(SpeedFieldWidth);
        float speed = m_camera.GetSpeed();
        std::string speedFormat = std::string(Text(EditorText::CameraSpeed)) + SpeedValueFormat;
        if (ImGui::DragFloat("##speed", &speed, SpeedDragStep, MinCameraSpeed, MaxCameraSpeed, speedFormat.c_str()))
        {
            m_camera.SetSpeed(speed);
        }
        if (m_application.GetContext().GetLevelObjectEditService().IsEditingLevelObject())
        {
            DrawLevelObjectEditorBar();
        }
    }

    void LevelViewPanel::DrawLevelObjectEditorBar()
    {
        EditorContext& context = m_application.GetContext();
        ImGui::SameLine();
        ImGui::TextColored(EditorColorVec4(EditorColor::Linked), "%s", Text(EditorText::EditingLevelObject, context.GetLevelService().GetEditLevel() ? context.GetLevelService().GetEditLevel()->GetName() : std::string()).c_str());
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::Save)))
        {
            context.GetLevelObjectEditService().SaveLevelObjectEditor();
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::BackToLevel)))
        {
            context.GetLevelObjectEditService().CloseLevelObjectEditor(true);
        }
    }

    void LevelViewPanel::DrawIcons(const Math::Matrix4& viewProjection, const ImVec2& origin, const ImVec2& size)
    {
        m_iconHovered = false;
        Level::Level* level = m_application.GetContext().GetLevelService().GetActiveLevel();
        if (!level)
        {
            return;
        }
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        EditorContext& context = m_application.GetContext();
        level->ForEachObject([&](Level::LevelObject& object)
        {
            if (object.GetHideInLevelContents() || !object.IsActiveInHierarchy())
            {
                return;
            }
            ObjectIcon icon = IconFor(object);
            if (!icon.glyph)
            {
                return;
            }
            const char* glyph = icon.glyph;
            ImU32 color = icon.color;
            Math::Vector4 clip = viewProjection.Transform(Math::Vector4(object.GetTransform().GetPosition(), 1.0f));
            if (clip.w <= IconMinClipW)
            {
                return;
            }
            ImVec2 screen(origin.x + (clip.x / clip.w * 0.5f + 0.5f) * size.x, origin.y + (0.5f - clip.y / clip.w * 0.5f) * size.y);
            float radius = IconRadius;
            bool hovered = ImGui::IsWindowHovered() && std::abs(ImGui::GetIO().MousePos.x - screen.x) < radius && std::abs(ImGui::GetIO().MousePos.y - screen.y) < radius;
            bool selected = context.GetSelectionService().IsSelected(object.GetId());
            drawList->AddCircleFilled(screen, radius, hovered ? EditorColorU32(EditorColor::ControlHovered) : EditorColorU32(EditorColor::GizmoBackground));
            drawList->AddCircle(screen, radius, selected ? EditorColorU32(EditorColor::Accent) : color, 0, selected ? IconSelectedOutline : IconOutline);
            ImVec2 textSize = ImGui::CalcTextSize(glyph);
            drawList->AddText(ImVec2(screen.x - textSize.x * 0.5f, screen.y - textSize.y * 0.5f - IconGlyphOffsetY), selected ? EditorColorU32(EditorColor::Accent) : color, glyph);
            if (hovered)
            {
                m_iconHovered = true;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver())
                {
                    context.GetSelectionService().Select(object.GetId(), ImGui::GetIO().KeyShift || ImGui::GetIO().KeySuper);
                }
            }
        });
    }

    void LevelViewPanel::DrawGizmo(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size)
    {
        EditorContext& context = m_application.GetContext();
        Level::LevelObject* object = context.GetSelectionService().GetPrimarySelection();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(origin.x, origin.y, size.x, size.y);
        if (!object)
        {
            m_gizmoActive = false;
            return;
        }
        bool lockedChild = object->IsLinked() && !object->GetLink().isRoot && !context.GetLevelObjectEditService().IsEditingLevelObject() && !context.GetLevelService().IsPlaying();
        if (lockedChild)
        {
            return;
        }
        const GizmoMode& gizmo = GizmoModes[static_cast<size_t>(m_gizmoOperation)];
        ImGuizmo::OPERATION operation = gizmo.operation;
        ImGuizmo::MODE mode = m_gizmoLocal || operation == ImGuizmo::SCALE ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        Math::Matrix4 world = object->GetTransform().GetWorldMatrix();
        float snapValues[3] = { gizmo.snap, gizmo.snap, gizmo.snap };
        bool snap = m_snapping != ImGui::GetIO().KeyCtrl;
        bool changed = ImGuizmo::Manipulate(view.Data(), projection.Data(), operation, mode, &world.m[0][0], nullptr, snap ? snapValues : nullptr);
        bool using_ = ImGuizmo::IsUsing();
        if (using_ && !m_gizmoActive)
        {
            m_gizmoActive = true;
            m_gizmoObject = object->GetId();
            m_gizmoBefore.SetNull();
            ObjectEditService::Snapshot(*object, m_gizmoBefore);
        }
        if (changed)
        {
            object->GetTransform().SetWorldMatrix(world);
        }
        if (!using_ && m_gizmoActive)
        {
            m_gizmoActive = false;
            if (!context.GetLevelService().IsPlaying())
            {
                context.GetUndoService().RecordObjectChange(m_gizmoObject, m_gizmoBefore, Text(EditorText::UndoTransform));
            }
        }
    }

    void LevelViewPanel::HandlePicking(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size)
    {
        if (!ImGui::IsWindowHovered() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::GetIO().KeyAlt || ImGuizmo::IsOver() || m_iconHovered)
        {
            return;
        }
        ImVec2 mouse = ImGui::GetIO().MousePos;
        if (mouse.x < origin.x || mouse.y < origin.y || mouse.x > origin.x + size.x || mouse.y > origin.y + size.y)
        {
            return;
        }
        EditorContext& context = m_application.GetContext();
        Level::Level* level = context.GetLevelService().GetActiveLevel();
        if (!level)
        {
            return;
        }
        PickResult hit = PickObject(*level, ScreenRay(view, projection, origin, size, mouse));
        bool additive = ImGui::GetIO().KeyShift || ImGui::GetIO().KeySuper;
        if (!hit.object)
        {
            if (!additive)
            {
                context.GetSelectionService().ClearSelection();
            }
            return;
        }
        Level::LevelObject* target = hit.object;
        if (!context.GetLevelObjectEditService().IsEditingLevelObject())
        {
            if (Level::LevelObject* linkRoot = target->GetLinkRoot())
            {
                if (!context.GetSelectionService().IsSelected(linkRoot->GetId()))
                {
                    target = linkRoot;
                }
            }
        }
        context.GetSelectionService().Select(target->GetId(), additive);
    }

    void LevelViewPanel::EnsureTarget(uint32_t width, uint32_t height)
    {
        if (m_target && m_target->GetWidth() == width && m_target->GetHeight() == height)
        {
            return;
        }
        m_application.GetDevice().WaitIdle();
        RHI::TextureDesc desc;
        desc.width = width;
        desc.height = height;
        desc.format = RHI::Format::RGBA8Unorm;
        desc.usage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
        desc.debugName = "LevelView";
        m_target = m_application.GetDevice().CreateTexture(desc);
    }

    void LevelViewPanel::UpdateCamera(float deltaTime, bool hovered)
    {
        m_camera.Update(deltaTime, hovered && !ImGuizmo::IsUsing(), ImGui::IsWindowFocused());
        if ((hovered || m_camera.IsFlying()) && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
        {
            ImGui::SetWindowFocus();
        }
    }

    void LevelViewPanel::SubmitRender(const Math::Matrix4& view, const Math::Matrix4& projection, uint32_t width, uint32_t height)
    {
        EditorPreferences& preferences = EditorPreferences::Get();
        ViewRenderRequest request;
        request.level = m_application.GetContext().GetLevelService().GetActiveLevel();
        request.target = m_target.get();
        request.view.id = LevelViewId;
        request.view.width = width;
        request.view.height = height;
        request.view.view = view;
        request.view.projection = projection;
        request.view.position = m_camera.GetPosition();
        request.view.verticalFov = m_camera.GetVerticalFov();
        request.view.nearPlane = m_camera.GetNearPlane();
        request.view.farPlane = m_camera.GetFarPlane();
        request.view.showGrid = m_showGrid;
        request.view.gridColor = preferences.GetColor(EditorColor::GridMinor);
        request.view.gridMajorColor = preferences.GetColor(EditorColor::GridMajor);
        request.view.gridAxisXColor = preferences.GetColor(EditorColor::AxisX);
        request.view.gridAxisZColor = preferences.GetColor(EditorColor::AxisZ);
        request.view.isEditorView = true;
        request.view.debugMode = m_debugMode;
        request.view.post.antiAliasing = Rendering::AntiAliasing::FXAA;
        request.view.post.autoExposure = false;
        m_application.RequestRender(request);
    }

    void LevelViewPanel::Draw()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        bool open = BeginWindow(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        if (!open)
        {
            ImGui::End();
            return;
        }
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + ToolbarOffsetX, ImGui::GetCursorPosY() + ToolbarOffsetY));
        DrawToolbar();

        EditorContext& context = m_application.GetContext();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < MinViewportSize || size.y < MinViewportSize || !context.GetLevelService().GetActiveLevel())
        {
            ImGui::End();
            return;
        }
        ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        uint32_t width = static_cast<uint32_t>(size.x * scale.x);
        uint32_t height = static_cast<uint32_t>(size.y * scale.y);
        EnsureTarget(width, height);

        bool hovered = ImGui::IsWindowHovered();
        UpdateCamera(ImGui::GetIO().DeltaTime, hovered);
        Math::Matrix4 view = m_camera.GetViewMatrix();
        Math::Matrix4 projection = m_camera.GetProjectionMatrix(size.x / size.y);

        SelectionOverlay::Draw(context.GetSelectionService());
        SubmitRender(view, projection, width, height);

        ImGui::Image(ImGuiRenderer::ToTextureId(m_target.get()), size);
        LevelViewDropTarget::Handle(context, ScreenRay(view, projection, origin, size, ImGui::GetIO().MousePos));

        ImGui::SetCursorScreenPos(origin);
        DrawIcons(view * projection, origin, size);
        DrawGizmo(view, projection, origin, size);
        HandlePicking(view, projection, origin, size);
        if (context.GetLevelService().IsPlaying())
        {
            PlayModeOverlay::Draw(origin, size);
        }
        if (hovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        {
            m_application.SetActivePanel(*this);
        }
        if ((hovered || ImGui::IsWindowFocused()) && !m_camera.IsFlying())
        {
            CommandRegistry::Get().DispatchShortcuts(CommandScope::LevelView);
        }
        ImGui::End();
    }
}
