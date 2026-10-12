#include "Panels/LevelContentsPanel.h"

#include "Assets/AssetDatabase.h"
#include "EditorApplication.h"
#include "EditorPlaySession.h"
#include "Framework/Camera.h"
#include "Framework/PlayerStart.h"
#include "Level/LevelSerializer.h"
#include "Panels/LevelViewPanel.h"
#include "Rendering/BuiltinAssets.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace BrightBox3D::Editor
{
    namespace
    {
        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), ::tolower);
            return text;
        }
    }

    LevelContentsPanel::LevelContentsPanel(EditorApplication& application) : m_application(application)
    {
    }

    bool LevelContentsPanel::MatchesFilter(const Level::LevelObjectInstance& object) const
    {
        if (m_filter.empty())
        {
            return true;
        }
        if (Lower(object.GetName()).find(Lower(m_filter)) != std::string::npos)
        {
            return true;
        }
        for (const Level::LevelObjectInstance* child : object.GetChildren())
        {
            if (MatchesFilter(*child))
            {
                return true;
            }
        }
        return false;
    }

    void LevelContentsPanel::DrawCreateMenu(Level::LevelObjectInstance* parent)
    {
        EditorContext& context = m_application.GetContext();
        Math::Vector3 spawn = parent ? parent->GetTransform().GetPosition() : m_application.GetLevelView().GetSpawnPoint();
        if (ImGui::MenuItem("Empty LevelObject"))
        {
            context.CreateObject(Level::LevelObjectInstance::StaticType(), "LevelObject", parent);
        }
        if (ImGui::BeginMenu("3D Object"))
        {
            struct Primitive
            {
                const char* name;
                Core::Guid guid;
            };
            Primitive primitives[] = {
                { "Cube", Rendering::BuiltinAssets::Cube() },
                { "Sphere", Rendering::BuiltinAssets::Sphere() },
                { "Capsule", Rendering::BuiltinAssets::Capsule() },
                { "Cylinder", Rendering::BuiltinAssets::Cylinder() },
                { "Plane", Rendering::BuiltinAssets::Plane() },
                { "Quad", Rendering::BuiltinAssets::Quad() }
            };
            for (const Primitive& primitive : primitives)
            {
                if (ImGui::MenuItem(primitive.name))
                {
                    context.CreateModelObject(primitive.guid, primitive.name, spawn, parent);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Light"))
        {
            if (ImGui::MenuItem("Directional Light"))
            {
                context.CreateLight(0);
            }
            if (ImGui::MenuItem("Point Light"))
            {
                context.CreateLight(1);
            }
            if (ImGui::MenuItem("Spot Light"))
            {
                context.CreateLight(2);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Gameplay"))
        {
            if (ImGui::MenuItem("Player Start"))
            {
                if (auto* object = context.CreateObject(Framework::PlayerStart::StaticType(), "PlayerStart", parent))
                {
                    object->GetTransform().SetPosition({ spawn.x, 0.0f, spawn.z });
                }
            }
            if (ImGui::MenuItem("Camera"))
            {
                if (auto* object = context.CreateObject(Framework::Camera::StaticType(), "Camera", parent))
                {
                    object->GetTransform().SetPosition(spawn + Math::Vector3(0.0f, 1.5f, 0.0f));
                }
            }
            ImGui::EndMenu();
        }
        std::vector<const Reflection::TypeInfo*> custom;
        for (const Reflection::TypeInfo* type : Reflection::TypeRegistry::GetDerived(Level::LevelObjectInstance::StaticType()))
        {
            if (type != &Level::LevelObjectInstance::StaticType() && type->GetModuleName() != "BrightBox3D")
            {
                custom.push_back(type);
            }
        }
        if (!custom.empty() && ImGui::BeginMenu("Game Classes"))
        {
            for (const Reflection::TypeInfo* type : custom)
            {
                if (ImGui::MenuItem(type->GetName().c_str()))
                {
                    context.CreateObject(*type, type->GetName(), parent);
                }
            }
            ImGui::EndMenu();
        }
    }

    void LevelContentsPanel::DrawObjectContextMenu(Level::LevelObjectInstance& object)
    {
        EditorContext& context = m_application.GetContext();
        if (!context.IsSelected(object.GetId()))
        {
            context.Select(object.GetId());
        }
        if (ImGui::MenuItem("Rename", "F2"))
        {
            m_renaming = object.GetId();
            m_renameBuffer = object.GetName();
            m_focusRename = true;
        }
        if (ImGui::MenuItem("Duplicate", "Cmd+D"))
        {
            context.DuplicateSelection();
        }
        if (ImGui::MenuItem("Delete", "Del"))
        {
            context.DeleteSelection();
            return;
        }
        if (ImGui::MenuItem("Focus", "F"))
        {
            m_application.GetLevelView().FocusSelection();
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Create Child"))
        {
            DrawCreateMenu(&object);
            ImGui::EndMenu();
        }
        if (!context.IsPlaying())
        {
            ImGui::Separator();
            if (object.GetLink().isRoot)
            {
                if (ImGui::MenuItem("Select LevelObject Asset"))
                {
                    context.SelectAsset(object.GetLink().asset);
                }
                if (ImGui::MenuItem("Open LevelObject"))
                {
                    context.OpenLevelObjectEditor(object.GetLink().asset);
                    return;
                }
                if (ImGui::MenuItem("Apply All Overrides"))
                {
                    Level::LevelSerializer::ApplyAllToAsset(object);
                }
                if (ImGui::MenuItem("Revert All Overrides"))
                {
                    Serialization::JsonDocument before;
                    EditorContext::Snapshot(object, before);
                    Core::Guid id = object.GetId();
                    Level::LevelSerializer::RevertAll(object);
                    context.RecordObjectChange(id, before, "Revert Overrides");
                    return;
                }
                if (ImGui::MenuItem("Unlink"))
                {
                    Serialization::JsonDocument before;
                    EditorContext::Snapshot(object, before);
                    Level::LevelSerializer::Unlink(object);
                    context.RecordObjectChange(object.GetId(), before, "Unlink");
                }
            }
            else if (!object.IsLinked() && !context.IsEditingLevelObject())
            {
                if (ImGui::MenuItem("Create LevelObject Asset"))
                {
                    context.CreateLevelObjectAsset(object, context.GetAssetsFolder());
                }
                if (ImGui::MenuItem("Create LevelObjectTemplate"))
                {
                    context.CreateTemplateAsset(&object, context.GetAssetsFolder());
                }
            }
        }
        else if (EditorPlaySession* session = m_application.GetPlaySession())
        {
            ImGui::Separator();
            bool keeping = session->IsKeepingChanges(object.GetId());
            if (ImGui::MenuItem("Keep Changes", nullptr, keeping))
            {
                session->ToggleKeepChanges(object.GetId());
            }
        }
    }

    void LevelContentsPanel::DrawObject(Level::LevelObjectInstance& object)
    {
        if (object.GetHideInLevelContents() || object.IsPendingDestroy() || !MatchesFilter(object))
        {
            return;
        }
        EditorContext& context = m_application.GetContext();
        ImGui::PushID(object.GetId().ToString().c_str());

        bool hasChildren = std::any_of(object.GetChildren().begin(), object.GetChildren().end(), [](const Level::LevelObjectInstance* child) { return !child->GetHideInLevelContents(); });
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        if (!hasChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (context.IsSelected(object.GetId()))
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!m_filter.empty())
        {
            ImGui::SetNextItemOpen(true);
        }
        for (const Core::Guid& selected : context.GetSelection())
        {
            Level::LevelObjectInstance* selectedObject = context.GetActiveLevel()->FindObject(selected);
            if (selectedObject && selectedObject->IsDescendantOf(&object) && m_scrollTo == selected)
            {
                ImGui::SetNextItemOpen(true);
            }
        }

        ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        if (!object.GetMissingTypeName().empty())
        {
            color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        }
        else if (object.IsLinked() && !context.IsEditingLevelObject())
        {
            color = ImVec4(0.45f, 0.72f, 1.0f, 1.0f);
        }
        if (!object.IsActiveInHierarchy())
        {
            color.w *= 0.5f;
        }
        bool keeping = context.IsPlaying() && m_application.GetPlaySession() && m_application.GetPlaySession()->IsKeepingChanges(object.GetId());

        bool open;
        if (m_renaming == object.GetId())
        {
            open = ImGui::TreeNodeEx("##node", flags, "%s", "");
            ImGui::SameLine();
            if (m_focusRename)
            {
                ImGui::SetKeyboardFocusHere();
                m_focusRename = false;
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##rename", &m_renameBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
            {
                Serialization::JsonDocument before;
                EditorContext::Snapshot(object, before);
                object.SetName(m_renameBuffer);
                context.RecordObjectChange(object.GetId(), before, "Rename");
                m_renaming = {};
            }
            else if (!ImGui::IsItemActive() && !m_focusRename && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Escape)))
            {
                m_renaming = {};
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            std::string label = object.GetName();
            if (!object.GetMissingTypeName().empty())
            {
                label += "  (missing " + object.GetMissingTypeName() + ")";
            }
            else if (&object.GetType() != &Level::LevelObjectInstance::StaticType())
            {
                label += "  [" + object.GetType().GetName() + "]";
            }
            if (keeping)
            {
                label += "  (keep changes)";
            }
            open = ImGui::TreeNodeEx("##node", flags, "%s", label.c_str());
            ImGui::PopStyleColor();
        }

        if (m_scrollTo == object.GetId())
        {
            ImGui::SetScrollHereY();
            m_scrollTo = {};
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        {
            context.Select(object.GetId(), ImGui::GetIO().KeyShift || ImGui::GetIO().KeySuper || ImGui::GetIO().KeyCtrl);
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            m_application.GetLevelView().FocusSelection();
        }
        if (ImGui::BeginDragDropSource())
        {
            Core::Guid id = object.GetId();
            ImGui::SetDragDropPayload("BRIGHT_OBJECT", &id, sizeof(id));
            ImGui::TextUnformatted(object.GetName().c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRIGHT_OBJECT"))
            {
                Core::Guid dragged = *static_cast<const Core::Guid*>(payload->Data);
                Level::LevelObjectInstance* draggedObject = context.GetActiveLevel()->FindObject(dragged);
                if (draggedObject && draggedObject != &object && !object.IsDescendantOf(draggedObject))
                {
                    if (ImGui::GetIO().KeyAlt)
                    {
                        context.Reparent(*draggedObject, object.GetParent(), object.GetCollection(), object.GetSiblingIndex());
                    }
                    else
                    {
                        context.Reparent(*draggedObject, &object, object.GetCollection(), -1);
                    }
                }
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRIGHT_ASSET"))
            {
                Core::Guid guid = *static_cast<const Core::Guid*>(payload->Data);
                const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(guid);
                if (record && record->type == Assets::AssetType::LevelObject)
                {
                    context.InstantiateLevelObject(guid, object.GetTransform().GetPosition(), &object);
                }
                else if (record && record->type == Assets::AssetType::Model)
                {
                    context.CreateModelObject(guid, record->name.substr(0, record->name.find('.')), object.GetTransform().GetPosition(), &object);
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("ObjectContext"))
        {
            DrawObjectContextMenu(object);
            ImGui::EndPopup();
        }

        if (open)
        {
            std::vector<Level::LevelObjectInstance*> children = object.GetChildren();
            for (Level::LevelObjectInstance* child : children)
            {
                DrawObject(*child);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void LevelContentsPanel::DrawCollection(Level::LevelCollection& collection)
    {
        EditorContext& context = m_application.GetContext();
        ImGui::PushID(&collection);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Framed;
        if (context.GetFocus() == SelectionFocus::Collection && context.GetSelectedCollection() == &collection)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        std::string label = collection.GetName();
        if (collection.IsDirty())
        {
            label += " *";
        }
        if (context.GetActiveCollection() == &collection && context.GetEditLevel() && context.GetEditLevel()->GetCollections().size() > 1)
        {
            label += "  (active)";
        }
        bool open = ImGui::TreeNodeEx("##collection", flags, "%s", label.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            context.SelectCollection(&collection);
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRIGHT_OBJECT"))
            {
                Core::Guid dragged = *static_cast<const Core::Guid*>(payload->Data);
                if (Level::LevelObjectInstance* draggedObject = context.GetActiveLevel()->FindObject(dragged))
                {
                    context.Reparent(*draggedObject, nullptr, &collection, -1);
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("CollectionContext"))
        {
            if (ImGui::MenuItem("Set Active Collection"))
            {
                context.SetActiveCollection(&collection);
            }
            if (ImGui::BeginMenu("Create"))
            {
                context.SetActiveCollection(&collection);
                DrawCreateMenu(nullptr);
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
        if (open)
        {
            std::vector<Level::LevelObjectInstance*> roots = collection.GetRoots();
            for (Level::LevelObjectInstance* root : roots)
            {
                DrawObject(*root);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void LevelContentsPanel::Draw()
    {
        if (!ImGui::Begin("Level Contents"))
        {
            ImGui::End();
            return;
        }
        EditorContext& context = m_application.GetContext();
        Level::Level* level = context.GetActiveLevel();
        if (!level)
        {
            ImGui::TextDisabled("No level open");
            ImGui::End();
            return;
        }
        if (ImGui::Button("+"))
        {
            ImGui::OpenPopup("CreatePopup");
        }
        if (ImGui::BeginPopup("CreatePopup"))
        {
            DrawCreateMenu(nullptr);
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##filter", "Search", &m_filter);

        std::string levelLabel = level->GetName() + (context.IsDirty() ? " *" : "");
        if (context.IsPlaying())
        {
            levelLabel += "  (PlayMode)";
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
        if (ImGui::Selectable(levelLabel.c_str(), context.GetFocus() == SelectionFocus::Level))
        {
            context.SelectLevel();
        }
        ImGui::PopStyleColor();
        ImGui::Separator();

        ImGui::BeginChild("Tree", ImVec2(0, 0), ImGuiChildFlags_None);
        for (Level::LevelObjectInstance* selected : context.GetSelectedObjects())
        {
            static Core::Guid lastSelected;
            if (selected->GetId() != lastSelected)
            {
                lastSelected = selected->GetId();
                m_scrollTo = selected->GetId();
            }
        }
        for (const auto& collection : level->GetCollections())
        {
            if (context.IsPlaying() && collection->GetName() == "Runtime" && collection->GetRoots().empty())
            {
                continue;
            }
            DrawCollection(*collection);
        }
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
        {
            context.ClearSelection();
        }
        if (ImGui::BeginPopupContextWindow("TreeContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawCreateMenu(nullptr);
            ImGui::EndPopup();
        }
        if (ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F2))
        {
            if (Level::LevelObjectInstance* primary = context.GetPrimarySelection())
            {
                m_renaming = primary->GetId();
                m_renameBuffer = primary->GetName();
                m_focusRename = true;
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }
}
