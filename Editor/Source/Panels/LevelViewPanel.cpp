#include "Panels/LevelViewPanel.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
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
#include "Physics/Colliders.h"
#include "Picking.h"
#include "Rendering/DebugDraw.h"
#include "Rendering/Light.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/MeshRenderer.h"

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
        Level::LevelObject* object = m_application.GetContext().GetSelectionService().GetPrimarySelection();
        if (!object)
        {
            return;
        }
        Math::Bounds bounds;
        std::function<void(Level::LevelObject&)> gather = [&](Level::LevelObject& node)
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
        Level::Level* level = m_application.GetContext().GetLevelService().GetActiveLevel();
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
        auto toggle = [](const std::string& text, bool active)
        {
            const char* label = text.c_str();
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
        if (toggle(std::string(IconGlyph(Icon::Move)) + "  Move", m_gizmoOperation == GizmoOperation::Translate))
        {
            m_gizmoOperation = GizmoOperation::Translate;
        }
        ImGui::SameLine();
        if (toggle(std::string(IconGlyph(Icon::RotateCw)) + "  Rotate", m_gizmoOperation == GizmoOperation::Rotate))
        {
            m_gizmoOperation = GizmoOperation::Rotate;
        }
        ImGui::SameLine();
        if (toggle(std::string(IconGlyph(Icon::Scaling)) + "  Scale", m_gizmoOperation == GizmoOperation::Scale))
        {
            m_gizmoOperation = GizmoOperation::Scale;
        }
        ImGui::SameLine();
        if (ImGui::Button((std::string(IconGlyph(Icon::Globe)) + (m_gizmoLocal ? "  Local" : "  World")).c_str()))
        {
            m_gizmoLocal = !m_gizmoLocal;
        }
        ImGui::SameLine();
        if (toggle(std::string(IconGlyph(Icon::Magnet)) + "  Snap", m_snapping))
        {
            m_snapping = !m_snapping;
        }
        ImGui::SameLine();
        if (toggle(std::string(IconGlyph(Icon::Grid)) + "  Grid", m_showGrid))
        {
            m_showGrid = !m_showGrid;
        }
        ImGui::SameLine();
        static const char* modes[] = { Text(EditorText::DebugModeLit), Text(EditorText::DebugModeAlbedo), Text(EditorText::DebugModeNormals), Text(EditorText::DebugModeMetallicRoughness), Text(EditorText::DebugModeShadows), Text(EditorText::DebugModeAmbientOcclusion) };
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
        if (context.GetLevelObjectEditService().IsEditingLevelObject())
        {
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
    }

    void LevelViewPanel::DrawSelectionOverlay()
    {
        EditorContext& context = m_application.GetContext();
        for (Level::LevelObject* object : context.GetSelectionService().GetSelectedObjects())
        {
            std::function<void(Level::LevelObject&)> visit = [&](Level::LevelObject& node)
            {
                if (auto* renderer = node.GetComponent<Rendering::MeshRenderer>())
                {
                    auto model = renderer->GetModel().Load();
                    if (const Geometry::MeshData* mesh = model ? model->GetMesh(renderer->GetMeshIndex()) : nullptr)
                    {
                        Math::Matrix4 box = Math::Matrix4::Translation(mesh->bounds.Center()) * node.GetTransform().GetWorldMatrix();
                        Rendering::DebugDraw::WireBox(box, mesh->bounds.Extents(), EditorPreferences::Get().GetColor(EditorColor::Accent), 0.0f, false);
                    }
                }
                if (auto* light = node.GetComponent<Rendering::Light>())
                {
                    const Level::Transform& transform = node.GetTransform();
                    Math::Color color = EditorPreferences::Get().GetColor(EditorColor::LightGizmo);
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
                for (Physics::Collider* collider : node.GetComponents<Physics::Collider>())
                {
                    Math::Color color = EditorPreferences::Get().GetColor(collider->IsTrigger() ? EditorColor::Trigger : EditorColor::Collider);
                    const Math::Matrix4& world = node.GetTransform().GetWorldMatrix();
                    Math::Vector3 center = world.TransformPoint(collider->GetCenter());
                    Math::Vector3 scale = node.GetTransform().GetLossyScale();
                    float uniform = std::max(scale.x, std::max(scale.y, scale.z));
                    if (auto* box = Reflection::Cast<Physics::BoxCollider>(collider))
                    {
                        Rendering::DebugDraw::WireBox(Math::Matrix4::Translation(collider->GetCenter()) * world, box->GetSize() * 0.5f, color, 0.0f, false);
                    }
                    else if (auto* sphere = Reflection::Cast<Physics::SphereCollider>(collider))
                    {
                        float radius = sphere->GetRadius() * uniform;
                        Rendering::DebugDraw::Circle(center, Math::Vector3::Up(), radius, color, 32, false);
                        Rendering::DebugDraw::Circle(center, Math::Vector3::Right(), radius, color, 32, false);
                        Rendering::DebugDraw::Circle(center, Math::Vector3::Forward(), radius, color, 32, false);
                    }
                    else if (auto* capsule = Reflection::Cast<Physics::CapsuleCollider>(collider))
                    {
                        float radius = capsule->GetRadius() * std::max(scale.x, scale.z);
                        float half = std::max(capsule->GetHeight() * 0.5f * scale.y - radius, 0.0f);
                        Math::Vector3 up = node.GetTransform().GetUp();
                        Rendering::DebugDraw::Circle(center + up * half, up, radius, color, 32, false);
                        Rendering::DebugDraw::Circle(center - up * half, up, radius, color, 32, false);
                        Math::Vector3 right = node.GetTransform().GetRight() * radius;
                        Math::Vector3 forward = node.GetTransform().GetForward() * radius;
                        for (const Math::Vector3& side : { right, -right, forward, -forward })
                        {
                            Rendering::DebugDraw::Line(center + up * half + side, center - up * half + side, color, 0.0f, false);
                        }
                    }
                }
                if (auto* camera = Reflection::Cast<Framework::Camera>(&node))
                {
                    Rendering::RenderView view = camera->BuildRenderView(16, 9, 0);
                    Math::Matrix4 limited = Math::Matrix4::PerspectiveFovLH(view.verticalFov * Math::DegreesToRadians, 16.0f / 9.0f, view.nearPlane, std::min(view.farPlane, 3.0f));
                    Rendering::DebugDraw::Frustum((view.view * limited).Inverse(), EditorPreferences::Get().GetColor(EditorColor::CameraGizmo), false);
                }
                for (Level::LevelObject* child : node.GetChildren())
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
            const char* glyph = nullptr;
            ImU32 color = 0;
            if (const Rendering::Light* light = object.GetComponent<Rendering::Light>())
            {
                glyph = light->GetLightType() == Rendering::LightType::Directional ? IconGlyph(Icon::Sun) : IconGlyph(Icon::Lightbulb);
                color = EditorColorU32(EditorColor::LightGizmo);
            }
            else if (object.IsA<Framework::Camera>())
            {
                glyph = IconGlyph(Icon::Video);
                color = EditorColorU32(EditorColor::CameraGizmo);
            }
            else if (object.IsA<Framework::PlayerStart>())
            {
                glyph = IconGlyph(Icon::Flag);
                color = EditorColorU32(EditorColor::PlayerStartGizmo);
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
            float radius = 12.0f;
            bool hovered = ImGui::IsWindowHovered() && std::abs(ImGui::GetIO().MousePos.x - screen.x) < radius && std::abs(ImGui::GetIO().MousePos.y - screen.y) < radius;
            bool selected = context.GetSelectionService().IsSelected(object.GetId());
            drawList->AddCircleFilled(screen, radius, hovered ? EditorColorU32(EditorColor::ControlHovered) : EditorColorU32(EditorColor::GizmoBackground));
            drawList->AddCircle(screen, radius, selected ? EditorColorU32(EditorColor::Accent) : color, 0, selected ? 2.5f : 1.5f);
            ImVec2 textSize = ImGui::CalcTextSize(glyph);
            drawList->AddText(ImVec2(screen.x - textSize.x * 0.5f, screen.y - textSize.y * 0.5f - 1.0f), selected ? EditorColorU32(EditorColor::Accent) : color, glyph);
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
        ImGuizmo::OPERATION operation = m_gizmoOperation == GizmoOperation::Translate ? ImGuizmo::TRANSLATE : (m_gizmoOperation == GizmoOperation::Rotate ? ImGuizmo::ROTATE : ImGuizmo::SCALE);
        ImGuizmo::MODE mode = m_gizmoLocal || operation == ImGuizmo::SCALE ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
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
                    context.GetObjectEditService().CreateModelObject(guid, name, point);
                }
                else if (record->type == Assets::AssetType::LevelObjectVariant)
                {
                    context.GetObjectEditService().InstantiateLevelObject(guid, point);
                }
                else if (record->type == Assets::AssetType::LevelObjectTemplate)
                {
                    Core::Log::Warning(Text(EditorText::CreateVariantFirst));
                }
                else if (record->type == Assets::AssetType::Material)
                {
                    Level::Level* level = context.GetLevelService().GetActiveLevel();
                    PickResult hit = level ? PickObject(*level, ray) : PickResult{};
                    if (hit.object)
                    {
                        if (auto* renderer = hit.object->GetComponent<Rendering::MeshRenderer>())
                        {
                            Serialization::JsonDocument before;
                            ObjectEditService::Snapshot(*hit.object, before);
                            renderer->SetMaterial(0, Assets::AssetReference<Rendering::MaterialAsset>(guid));
                            context.GetUndoService().RecordObjectChange(hit.object->GetId(), before, Text(EditorText::UndoAssignMaterial));
                        }
                    }
                }
                else if (record->type == Assets::AssetType::Level)
                {
                    context.GetLevelService().OpenLevel(guid);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    void LevelViewPanel::Draw(float deltaTime)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        bool open = ImGui::Begin(WindowTitle(EditorWindow::LevelView).c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
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
        if (size.x < 8.0f || size.y < 8.0f || !context.GetLevelService().GetActiveLevel())
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
        request.level = context.GetLevelService().GetActiveLevel();
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
        request.view.gridColor = EditorPreferences::Get().GetColor(EditorColor::GridMinor);
        request.view.gridMajorColor = EditorPreferences::Get().GetColor(EditorColor::GridMajor);
        request.view.gridAxisXColor = EditorPreferences::Get().GetColor(EditorColor::AxisX);
        request.view.gridAxisZColor = EditorPreferences::Get().GetColor(EditorColor::AxisZ);
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

        if (context.GetLevelService().IsPlaying())
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), EditorColorU32(EditorColor::PlayMode, 0.8f), 0.0f, 0, 2.0f);
            const char* banner = Text(EditorText::PlayModeBanner);
            ImVec2 textSize = ImGui::CalcTextSize(banner);
            drawList->AddRectFilled(ImVec2(origin.x + 6.0f, origin.y + 6.0f), ImVec2(origin.x + 18.0f + textSize.x, origin.y + 12.0f + textSize.y), EditorColorU32(EditorColor::PlayMode, 0.85f), 4.0f);
            drawList->AddText(ImVec2(origin.x + 12.0f, origin.y + 9.0f), EditorColorU32(EditorColor::Text), banner);
        }

        if ((hovered || ImGui::IsWindowFocused()) && !m_camera.IsFlying() && !ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_W))
            {
                m_gizmoOperation = GizmoOperation::Translate;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_E))
            {
                m_gizmoOperation = GizmoOperation::Rotate;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_R))
            {
                m_gizmoOperation = GizmoOperation::Scale;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F))
            {
                FocusSelection();
            }
        }
        ImGui::End();
    }
}
