#include "Panels/LevelViewPanel.h"

#include "Assets/AssetDatabase.h"
#include "EditorApplication.h"
#include "EditorPlaySession.h"
#include "Framework/Camera.h"
#include "Framework/PlayerStart.h"
#include "ImGui/ImGuiRenderer.h"
#include "Level/LevelSerializer.h"
#include "Picking.h"
#include "Rendering/DebugDraw.h"
#include "Rendering/Light.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/ModelAsset.h"

#include <ImGuizmo.h>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr uint64_t LevelViewId = 0x4C455645;
    }

    LevelViewPanel::LevelViewPanel(EditorApplication& application) : m_application(application)
    {
    }

    LevelViewPanel::~LevelViewPanel()
    {
        m_application.GetDevice().WaitIdle();
        m_application.GetRenderer().ReleaseView(LevelViewId);
    }

    Math::Vector3 LevelViewPanel::GetSpawnPoint() const
    {
        return m_camera.GetPosition() + m_camera.GetForward() * 6.0f;
    }

    void LevelViewPanel::FocusSelection()
    {
        Level::LevelObjectInstance* object = m_application.GetContext().GetPrimarySelection();
        if (!object)
        {
            return;
        }
        Math::Bounds bounds;
        std::function<void(Level::LevelObjectInstance&)> gather = [&](Level::LevelObjectInstance& node)
        {
            if (auto* renderer = node.GetComponent<Rendering::MeshRenderer>())
            {
                auto model = renderer->GetModel().Load();
                if (const Rendering::MeshData* mesh = model ? model->GetMesh(renderer->GetMeshIndex()) : nullptr)
                {
                    bounds.Encapsulate(mesh->bounds.Transformed(node.GetTransform().GetWorldMatrix()));
                }
            }
            for (Level::LevelObjectInstance* child : node.GetChildren())
            {
                gather(*child);
            }
        };
        gather(*object);
        if (bounds.IsValid())
        {
            m_camera.Focus(bounds.Center(), bounds.Extents().Length());
        }
        else
        {
            m_camera.Focus(object->GetTransform().GetPosition(), 1.0f);
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

    Math::Vector3 LevelViewPanel::PlacementPoint(const Math::Ray& ray) const
    {
        Level::Level* level = m_application.GetContext().GetActiveLevel();
        if (level)
        {
            PickResult hit = PickObject(*level, ray);
            if (hit.object)
            {
                return hit.point;
            }
        }
        Math::Plane ground{ { 0.0f, 1.0f, 0.0f }, 0.0f };
        float distance = 0.0f;
        if (ground.Raycast(ray, distance) && distance < 500.0f)
        {
            return ray.GetPoint(distance);
        }
        return ray.GetPoint(8.0f);
    }

    void LevelViewPanel::DrawToolbar()
    {
        EditorContext& context = m_application.GetContext();
        auto toggle = [](const char* label, bool active)
        {
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }
            bool pressed = ImGui::Button(label);
            if (active)
            {
                ImGui::PopStyleColor();
            }
            return pressed;
        };
        if (toggle("Move", context.GetGizmoOperation() == GizmoOperation::Translate))
        {
            context.SetGizmoOperation(GizmoOperation::Translate);
        }
        ImGui::SameLine();
        if (toggle("Rotate", context.GetGizmoOperation() == GizmoOperation::Rotate))
        {
            context.SetGizmoOperation(GizmoOperation::Rotate);
        }
        ImGui::SameLine();
        if (toggle("Scale", context.GetGizmoOperation() == GizmoOperation::Scale))
        {
            context.SetGizmoOperation(GizmoOperation::Scale);
        }
        ImGui::SameLine();
        if (ImGui::Button(context.GetGizmoLocal() ? "Local" : "World"))
        {
            context.SetGizmoLocal(!context.GetGizmoLocal());
        }
        ImGui::SameLine();
        if (toggle("Snap", context.GetSnapping()))
        {
            context.SetSnapping(!context.GetSnapping());
        }
        ImGui::SameLine();
        if (toggle("Grid", m_showGrid))
        {
            m_showGrid = !m_showGrid;
        }
        ImGui::SameLine();
        static const char* modes[] = { "Lit", "Albedo", "Normals", "Metal/Rough", "Shadows", "AO" };
        ImGui::SetNextItemWidth(110.0f);
        int mode = static_cast<int>(m_debugMode);
        if (ImGui::Combo("##debugmode", &mode, modes, IM_ARRAYSIZE(modes)))
        {
            m_debugMode = static_cast<Rendering::RenderDebugMode>(mode);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        float speed = m_camera.GetSpeed();
        if (ImGui::DragFloat("##speed", &speed, 0.1f, 0.5f, 500.0f, "Speed %.1f"))
        {
            m_camera.SetSpeed(speed);
        }
        if (context.IsEditingLevelObject())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.55f, 0.78f, 1.0f, 1.0f), "Editing %s", context.GetEditLevel() ? context.GetEditLevel()->GetName().c_str() : "");
            ImGui::SameLine();
            if (ImGui::Button("Save"))
            {
                context.SaveLevelObjectEditor();
            }
            ImGui::SameLine();
            if (ImGui::Button("Back to Level"))
            {
                context.CloseLevelObjectEditor(true);
            }
        }
    }

    void LevelViewPanel::DrawSelectionOverlay()
    {
        EditorContext& context = m_application.GetContext();
        for (Level::LevelObjectInstance* object : context.GetSelectedObjects())
        {
            std::function<void(Level::LevelObjectInstance&)> visit = [&](Level::LevelObjectInstance& node)
            {
                if (auto* renderer = node.GetComponent<Rendering::MeshRenderer>())
                {
                    auto model = renderer->GetModel().Load();
                    if (const Rendering::MeshData* mesh = model ? model->GetMesh(renderer->GetMeshIndex()) : nullptr)
                    {
                        Math::Matrix4 box = Math::Matrix4::Translation(mesh->bounds.Center()) * node.GetTransform().GetWorldMatrix();
                        Rendering::DebugDraw::WireBox(box, mesh->bounds.Extents(), { 1.0f, 0.62f, 0.15f, 1.0f }, 0.0f, false);
                    }
                }
                if (auto* light = node.GetComponent<Rendering::Light>())
                {
                    const Level::Transform& transform = node.GetTransform();
                    Math::Color color{ 1.0f, 0.9f, 0.4f, 0.9f };
                    if (light->GetLightType() == Rendering::LightType::Point)
                    {
                        Rendering::DebugDraw::Circle(transform.GetPosition(), Math::Vector3::Up(), light->GetRange(), color, 48, false);
                        Rendering::DebugDraw::Circle(transform.GetPosition(), Math::Vector3::Right(), light->GetRange(), color, 48, false);
                        Rendering::DebugDraw::Circle(transform.GetPosition(), Math::Vector3::Forward(), light->GetRange(), color, 48, false);
                    }
                    else if (light->GetLightType() == Rendering::LightType::Spot)
                    {
                        Rendering::DebugDraw::Cone(transform.GetPosition(), transform.GetForward(), light->GetRange(), light->GetOuterAngle(), color, false);
                    }
                    else
                    {
                        Math::Vector3 start = transform.GetPosition();
                        Rendering::DebugDraw::Line(start, start + transform.GetForward() * 2.0f, color, 0.0f, false);
                        Rendering::DebugDraw::Circle(start, transform.GetForward(), 0.4f, color, 24, false);
                    }
                }
                if (auto* camera = Reflection::Cast<Framework::Camera>(&node))
                {
                    Rendering::RenderView view = camera->BuildRenderView(16, 9, 0);
                    Math::Matrix4 limited = Math::Matrix4::PerspectiveFovLH(view.verticalFov * Math::DegreesToRadians, 16.0f / 9.0f, view.nearPlane, std::min(view.farPlane, 3.0f));
                    Rendering::DebugDraw::Frustum((view.view * limited).Inverse(), { 0.8f, 0.8f, 0.9f, 1.0f }, false);
                }
                for (Level::LevelObjectInstance* child : node.GetChildren())
                {
                    visit(*child);
                }
            };
            visit(*object);
        }
    }

    void LevelViewPanel::DrawIcons(const Math::Matrix4& viewProjection, const ImVec2& origin, const ImVec2& size)
    {
        m_iconHovered = false;
        Level::Level* level = m_application.GetContext().GetActiveLevel();
        if (!level)
        {
            return;
        }
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        EditorContext& context = m_application.GetContext();
        level->ForEachObject([&](Level::LevelObjectInstance& object)
        {
            if (object.GetHideInLevelContents() || !object.IsActiveInHierarchy())
            {
                return;
            }
            const char* glyph = nullptr;
            ImU32 color = IM_COL32(255, 230, 120, 230);
            if (object.GetComponent<Rendering::Light>())
            {
                glyph = "L";
            }
            else if (object.IsA<Framework::Camera>())
            {
                glyph = "C";
                color = IM_COL32(200, 210, 255, 230);
            }
            else if (object.IsA<Framework::PlayerStart>())
            {
                glyph = "P";
                color = IM_COL32(120, 240, 140, 230);
            }
            if (!glyph)
            {
                return;
            }
            Math::Vector4 clip = viewProjection.Transform(Math::Vector4(object.GetTransform().GetPosition(), 1.0f));
            if (clip.w <= 0.1f)
            {
                return;
            }
            ImVec2 screen(origin.x + (clip.x / clip.w * 0.5f + 0.5f) * size.x, origin.y + (0.5f - clip.y / clip.w * 0.5f) * size.y);
            float radius = 10.0f;
            bool hovered = ImGui::IsWindowHovered() && std::abs(ImGui::GetIO().MousePos.x - screen.x) < radius && std::abs(ImGui::GetIO().MousePos.y - screen.y) < radius;
            bool selected = context.IsSelected(object.GetId());
            drawList->AddCircleFilled(screen, radius, selected ? IM_COL32(255, 160, 40, 240) : (hovered ? IM_COL32(90, 90, 90, 230) : IM_COL32(40, 40, 40, 200)));
            drawList->AddCircle(screen, radius, color, 0, 1.5f);
            ImVec2 textSize = ImGui::CalcTextSize(glyph);
            drawList->AddText(ImVec2(screen.x - textSize.x * 0.5f, screen.y - textSize.y * 0.5f), color, glyph);
            if (hovered)
            {
                m_iconHovered = true;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver())
                {
                    context.Select(object.GetId(), ImGui::GetIO().KeyShift || ImGui::GetIO().KeySuper);
                }
            }
        });
    }

    void LevelViewPanel::DrawGizmo(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size)
    {
        EditorContext& context = m_application.GetContext();
        Level::LevelObjectInstance* object = context.GetPrimarySelection();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(origin.x, origin.y, size.x, size.y);
        if (!object)
        {
            m_gizmoActive = false;
            return;
        }
        bool lockedChild = object->IsLinked() && !object->GetLink().isRoot && !context.IsEditingLevelObject() && !context.IsPlaying();
        if (lockedChild)
        {
            return;
        }
        ImGuizmo::OPERATION operation = context.GetGizmoOperation() == GizmoOperation::Translate ? ImGuizmo::TRANSLATE : (context.GetGizmoOperation() == GizmoOperation::Rotate ? ImGuizmo::ROTATE : ImGuizmo::SCALE);
        ImGuizmo::MODE mode = context.GetGizmoLocal() || operation == ImGuizmo::SCALE ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        Math::Matrix4 world = object->GetTransform().GetWorldMatrix();
        float snapValues[3] = { 0.5f, 0.5f, 0.5f };
        if (operation == ImGuizmo::ROTATE)
        {
            snapValues[0] = snapValues[1] = snapValues[2] = 15.0f;
        }
        else if (operation == ImGuizmo::SCALE)
        {
            snapValues[0] = snapValues[1] = snapValues[2] = 0.1f;
        }
        bool snap = context.GetSnapping() != ImGui::GetIO().KeyCtrl;
        bool changed = ImGuizmo::Manipulate(view.Data(), projection.Data(), operation, mode, &world.m[0][0], nullptr, snap ? snapValues : nullptr);
        bool using_ = ImGuizmo::IsUsing();
        if (using_ && !m_gizmoActive)
        {
            m_gizmoActive = true;
            m_gizmoObject = object->GetId();
            m_gizmoBefore.SetNull();
            EditorContext::Snapshot(*object, m_gizmoBefore);
        }
        if (changed)
        {
            object->GetTransform().SetWorldMatrix(world);
        }
        if (!using_ && m_gizmoActive)
        {
            m_gizmoActive = false;
            if (!context.IsPlaying())
            {
                context.RecordObjectChange(m_gizmoObject, m_gizmoBefore, "Transform");
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
        Level::Level* level = context.GetActiveLevel();
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
                context.ClearSelection();
            }
            return;
        }
        Level::LevelObjectInstance* target = hit.object;
        if (!context.IsEditingLevelObject())
        {
            if (Level::LevelObjectInstance* linkRoot = target->GetLinkRoot())
            {
                if (!context.IsSelected(linkRoot->GetId()))
                {
                    target = linkRoot;
                }
            }
        }
        context.Select(target->GetId(), additive);
    }

    void LevelViewPanel::HandleDrop(const Math::Matrix4& view, const Math::Matrix4& projection, const ImVec2& origin, const ImVec2& size)
    {
        if (!ImGui::BeginDragDropTarget())
        {
            return;
        }
        EditorContext& context = m_application.GetContext();
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRIGHT_ASSET"))
        {
            Core::Guid guid = *static_cast<const Core::Guid*>(payload->Data);
            const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(guid);
            Math::Ray ray = ScreenRay(view, projection, origin, size, ImGui::GetIO().MousePos);
            Math::Vector3 point = PlacementPoint(ray);
            if (record)
            {
                std::string name = record->name.substr(0, record->name.find('.'));
                if (record->type == Assets::AssetType::Model)
                {
                    context.CreateModelObject(guid, name, point);
                }
                else if (record->type == Assets::AssetType::LevelObject)
                {
                    context.InstantiateLevelObject(guid, point);
                }
                else if (record->type == Assets::AssetType::LevelObjectTemplate)
                {
                    Core::Log::Warning("LevelObjectTemplates cannot be placed; create a LevelObject from it first");
                }
                else if (record->type == Assets::AssetType::Material)
                {
                    Level::Level* level = context.GetActiveLevel();
                    PickResult hit = level ? PickObject(*level, ray) : PickResult{};
                    if (hit.object)
                    {
                        if (auto* renderer = hit.object->GetComponent<Rendering::MeshRenderer>())
                        {
                            Serialization::JsonDocument before;
                            EditorContext::Snapshot(*hit.object, before);
                            renderer->SetMaterial(0, Assets::AssetReference<Rendering::MaterialAsset>(guid));
                            context.RecordObjectChange(hit.object->GetId(), before, "Assign Material");
                        }
                    }
                }
                else if (record->type == Assets::AssetType::Level)
                {
                    context.OpenLevel(guid);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    void LevelViewPanel::Draw(float deltaTime)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        bool open = ImGui::Begin("Level View", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        if (!open)
        {
            ImGui::End();
            return;
        }
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 6.0f, ImGui::GetCursorPosY() + 4.0f));
        DrawToolbar();

        EditorContext& context = m_application.GetContext();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 8.0f || size.y < 8.0f || !context.GetActiveLevel())
        {
            ImGui::End();
            return;
        }
        ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        uint32_t width = static_cast<uint32_t>(size.x * scale.x);
        uint32_t height = static_cast<uint32_t>(size.y * scale.y);
        if (!m_target || m_target->GetWidth() != width || m_target->GetHeight() != height)
        {
            m_application.GetDevice().WaitIdle();
            RHI::TextureDesc desc;
            desc.width = width;
            desc.height = height;
            desc.format = RHI::Format::RGBA8Unorm;
            desc.usage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
            desc.debugName = "LevelView";
            m_target = m_application.GetDevice().CreateTexture(desc);
        }

        bool hovered = ImGui::IsWindowHovered();
        m_camera.Update(deltaTime, hovered && !ImGuizmo::IsUsing(), ImGui::IsWindowFocused());
        if ((hovered || m_camera.IsFlying()) && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
        {
            ImGui::SetWindowFocus();
        }

        float aspect = size.x / size.y;
        Math::Matrix4 view = m_camera.GetViewMatrix();
        Math::Matrix4 projection = m_camera.GetProjectionMatrix(aspect);

        DrawSelectionOverlay();

        ViewRenderRequest request;
        request.level = context.GetActiveLevel();
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
        request.view.isEditorView = true;
        request.view.debugMode = m_debugMode;
        request.view.post.antiAliasing = Rendering::AntiAliasing::FXAA;
        request.view.post.autoExposure = false;
        m_application.RequestRender(request);

        ImGui::Image(ImGuiRenderer::ToTextureId(m_target.get()), size);
        HandleDrop(view, projection, origin, size);

        ImGui::SetCursorScreenPos(origin);
        DrawIcons(view * projection, origin, size);
        DrawGizmo(view, projection, origin, size);
        HandlePicking(view, projection, origin, size);

        if (context.IsPlaying())
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(60, 140, 255, 200), 0.0f, 0, 2.0f);
            drawList->AddText(ImVec2(origin.x + 10.0f, origin.y + 8.0f), IM_COL32(120, 180, 255, 255), "PlayMode - editing live objects (changes revert on stop)");
        }

        if ((hovered || ImGui::IsWindowFocused()) && !m_camera.IsFlying() && !ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_W))
            {
                context.SetGizmoOperation(GizmoOperation::Translate);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_E))
            {
                context.SetGizmoOperation(GizmoOperation::Rotate);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_R))
            {
                context.SetGizmoOperation(GizmoOperation::Scale);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F))
            {
                FocusSelection();
            }
        }
        ImGui::End();
    }
}
