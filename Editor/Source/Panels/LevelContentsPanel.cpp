#include "Panels/LevelContentsPanel.h"

#include "Assets/AssetDatabase.h"
#include "Commands/CommandRegistry.h"
#include "Commands/CommandWidgets.h"
#include "Core/EngineInfo.h"
#include "Core/Log.h"
#include "EditorApplication.h"
#include "EditorPlaySession.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "EditorWindows.h"
#include "Framework/Camera.h"
#include "Framework/PlayerStart.h"
#include "IconGlyphs.h"
#include "Level/LevelSerializer.h"
#include "ObjectIcons.h"
#include "Panels/LevelViewPanel.h"
#include "Physics/Colliders.h"
#include "Reflection/TypeRegistry.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/Light.h"
#include "Rendering/MeshRenderer.h"
#include "Theme.h"

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

        void DrawRowLabel(const ImVec2& position, const char* icon, const ImVec4& iconColor, const std::string& name, const ImVec4& nameColor, const std::string& suffix)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float iconWidth = ImGui::CalcTextSize(icon).x;
            drawList->AddText(position, ImGui::ColorConvertFloat4ToU32(iconColor), icon);
            ImVec2 namePosition(position.x + iconWidth + ImGui::GetStyle().ItemInnerSpacing.x + 2.0f, position.y);
            drawList->AddText(namePosition, ImGui::ColorConvertFloat4ToU32(nameColor), name.c_str());
            if (!suffix.empty())
            {
                ImVec2 suffixPosition(namePosition.x + ImGui::CalcTextSize(name.c_str()).x + ImGui::GetStyle().ItemSpacing.x, position.y);
                drawList->AddText(suffixPosition, EditorColorU32(EditorColor::TextDim), suffix.c_str());
            }
        }

        void DrawRowTag(float y, const char* tag)
        {
            float x = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - ImGui::CalcTextSize(tag).x;
            ImGui::GetWindowDrawList()->AddText(ImVec2(x, y), EditorColorU32(EditorColor::TextDim, 0.7f), tag);
        }
    }

    void LevelContentsPanel::DrawRowBackground(bool selected)
    {
        ImVec2 windowPosition = ImGui::GetWindowPos();
        float y = ImGui::GetCursorScreenPos().y;
        ImVec2 min(windowPosition.x, y);
        ImVec2 max(windowPosition.x + ImGui::GetWindowWidth(), y + ImGui::GetFrameHeight());
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(ImVec2(windowPosition.x, windowPosition.y), ImVec2(max.x, windowPosition.y + ImGui::GetWindowHeight()), false);
        if (m_rowIndex % 2 == 1)
        {
            drawList->AddRectFilled(min, max, EditorColorU32(EditorColor::RowStripe));
        }
        if (selected)
        {
            drawList->AddRectFilled(min, ImVec2(min.x + 2.0f, max.y), EditorColorU32(EditorColor::Accent));
        }
        drawList->PopClipRect();
        ++m_rowIndex;
    }

    LevelContentsPanel::LevelContentsPanel(EditorApplication& application) : m_application(application)
    {
    }

    bool LevelContentsPanel::MatchesFilter(const Level::LevelObject& object) const
    {
        if (m_filter.empty())
        {
            return true;
        }
        if (Lower(object.GetName()).find(Lower(m_filter)) != std::string::npos)
        {
            return true;
        }
        for (const Level::LevelObject* child : object.GetChildren())
        {
            if (MatchesFilter(*child))
            {
                return true;
            }
        }
        return false;
    }

    void LevelContentsPanel::DrawCreateMenu(Level::LevelObject* parent)
    {
        EditorContext& context = m_application.GetContext();
        Math::Vector3 spawn = parent ? parent->GetTransform().GetPosition() : m_application.GetActiveLevelView().GetSpawnPoint();
        if (ImGui::MenuItem(Text(EditorText::EmptyLevelObject)))
        {
            context.GetObjectEditService().CreateObject(Level::LevelObject::StaticType(), Level::LevelObject::StaticType().GetName(), parent);
        }
        if (ImGui::BeginMenu(Text(EditorText::CreateMenu3DObject)))
        {
            struct Primitive
            {
                EditorText name;
                Core::Guid guid;
            };
            Primitive primitives[] = {
                { EditorText::PrimitiveCube, Rendering::BuiltinAssets::Cube() },
                { EditorText::PrimitiveSphere, Rendering::BuiltinAssets::Sphere() },
                { EditorText::PrimitiveCapsule, Rendering::BuiltinAssets::Capsule() },
                { EditorText::PrimitiveCylinder, Rendering::BuiltinAssets::Cylinder() },
                { EditorText::PrimitivePlane, Rendering::BuiltinAssets::Plane() },
                { EditorText::PrimitiveQuad, Rendering::BuiltinAssets::Quad() }
            };
            for (const Primitive& primitive : primitives)
            {
                if (ImGui::MenuItem(Text(primitive.name)))
                {
                    context.GetObjectEditService().CreateModelObject(primitive.guid, Text(primitive.name), spawn, parent);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(Text(EditorText::CreateMenuLight)))
        {
            if (ImGui::MenuItem(Text(EditorText::DirectionalLight)))
            {
                context.GetObjectEditService().CreateLight(0);
            }
            if (ImGui::MenuItem(Text(EditorText::PointLight)))
            {
                context.GetObjectEditService().CreateLight(1);
            }
            if (ImGui::MenuItem(Text(EditorText::SpotLight)))
            {
                context.GetObjectEditService().CreateLight(2);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(Text(EditorText::CreateMenuGameplay)))
        {
            if (ImGui::MenuItem(Text(EditorText::PlayerStart)))
            {
                if (auto* object = context.GetObjectEditService().CreateObject(Framework::PlayerStart::StaticType(), Framework::PlayerStart::StaticType().GetName(), parent))
                {
                    object->GetTransform().SetPosition({ spawn.x, 0.0f, spawn.z });
                }
            }
            if (ImGui::MenuItem(Text(EditorText::Camera)))
            {
                if (auto* object = context.GetObjectEditService().CreateObject(Framework::Camera::StaticType(), Framework::Camera::StaticType().GetName(), parent))
                {
                    object->GetTransform().SetPosition(spawn + Math::Vector3(0.0f, 1.5f, 0.0f));
                }
            }
            ImGui::EndMenu();
        }
        std::vector<const Reflection::TypeInfo*> custom;
        for (const Reflection::TypeInfo* type : Reflection::TypeRegistry::GetDerived(Level::LevelObject::StaticType()))
        {
            if (type != &Level::LevelObject::StaticType() && type->GetModuleName() != Core::EngineModuleName)
            {
                custom.push_back(type);
            }
        }
        if (!custom.empty() && ImGui::BeginMenu(Text(EditorText::CreateMenuGameClasses)))
        {
            for (const Reflection::TypeInfo* type : custom)
            {
                if (ImGui::MenuItem(type->GetName().c_str()))
                {
                    context.GetObjectEditService().CreateObject(*type, type->GetName(), parent);
                }
            }
            ImGui::EndMenu();
        }
    }

    void LevelContentsPanel::BeginRenameSelection()
    {
        if (Level::LevelObject* primary = m_application.GetContext().GetSelectionService().GetPrimarySelection())
        {
            m_renaming = primary->GetId();
            m_renameBuffer = primary->GetName();
            m_focusRename = true;
        }
    }

    void LevelContentsPanel::DrawObjectContextMenu(Level::LevelObject& object)
    {
        EditorContext& context = m_application.GetContext();
        if (!context.GetSelectionService().IsSelected(object.GetId()))
        {
            context.GetSelectionService().Select(object.GetId());
        }
        CommandMenuItem(CommandId::Rename);
        CommandMenuItem(CommandId::Duplicate);
        if (CommandMenuItem(CommandId::Delete))
        {
            return;
        }
        CommandMenuItem(CommandId::FocusSelection);
        ImGui::Separator();
        if (ImGui::BeginMenu(Text(EditorText::CreateChild)))
        {
            DrawCreateMenu(&object);
            ImGui::EndMenu();
        }
        if (!context.GetLevelService().IsPlaying())
        {
            ImGui::Separator();
            if (object.GetLink().isRoot)
            {
                if (ImGui::MenuItem(Text(EditorText::SelectLevelObjectAsset)))
                {
                    context.GetSelectionService().SelectAsset(object.GetLink().asset);
                }
                if (ImGui::MenuItem(Text(EditorText::OpenLevelObject)))
                {
                    context.GetLevelObjectEditService().OpenLevelObjectEditor(object.GetLink().asset);
                    return;
                }
                if (ImGui::MenuItem(Text(EditorText::ApplyAllOverrides)))
                {
                    Level::LevelSerializer::ApplyAllToAsset(object);
                }
                if (ImGui::MenuItem(Text(EditorText::RevertAllOverrides)))
                {
                    Serialization::JsonDocument before;
                    ObjectEditService::Snapshot(object, before);
                    Core::Guid id = object.GetId();
                    Level::LevelSerializer::RevertAll(object);
                    context.GetUndoService().RecordObjectChange(id, before, Text(EditorText::UndoRevertOverrides));
                    return;
                }
                if (ImGui::MenuItem(Text(EditorText::Unlink)))
                {
                    Serialization::JsonDocument before;
                    ObjectEditService::Snapshot(object, before);
                    Level::LevelSerializer::Unlink(object);
                    context.GetUndoService().RecordObjectChange(object.GetId(), before, Text(EditorText::Unlink));
                }
            }
            else if (!object.IsLinked() && !context.GetLevelObjectEditService().IsEditingLevelObject())
            {
                if (ImGui::MenuItem(Text(EditorText::CreateLevelObjectVariant)))
                {
                    context.GetAssetCreationService().CreateVariantAsset(object, context.GetAssetCreationService().GetAssetsFolder());
                }
                if (ImGui::MenuItem(Text(EditorText::CreateLevelObjectTemplate)))
                {
                    context.GetAssetCreationService().CreateTemplateAsset(&object, context.GetAssetCreationService().GetAssetsFolder());
                }
            }
        }
        else if (EditorPlaySession* session = m_application.GetPlaySession())
        {
            ImGui::Separator();
            bool keeping = session->IsKeepingChanges(object.GetId());
            if (ImGui::MenuItem(Text(EditorText::KeepChanges), nullptr, keeping))
            {
                session->ToggleKeepChanges(object.GetId());
            }
        }
    }

    void LevelContentsPanel::DrawObject(Level::LevelObject& object)
    {
        if (object.GetHideInLevelContents() || object.IsPendingDestroy() || !MatchesFilter(object))
        {
            return;
        }
        EditorContext& context = m_application.GetContext();
        ImGui::PushID(object.GetId().ToString().c_str());

        bool hasChildren = std::any_of(object.GetChildren().begin(), object.GetChildren().end(), [](const Level::LevelObject* child) { return !child->GetHideInLevelContents(); });
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
        if (!hasChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (context.GetSelectionService().IsSelected(object.GetId()))
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!m_filter.empty())
        {
            ImGui::SetNextItemOpen(true);
        }
        for (const Core::Guid& selected : context.GetSelectionService().GetSelection())
        {
            Level::LevelObject* selectedObject = context.GetLevelService().GetActiveLevel()->FindObject(selected);
            if (selectedObject && selectedObject->IsDescendantOf(&object) && m_scrollTo == selected)
            {
                ImGui::SetNextItemOpen(true);
            }
        }

        ImVec4 color = EditorColorVec4(EditorColor::Text);
        ImVec4 iconColor = EditorColorVec4(EditorColor::TextDim);
        const char* icon = IconGlyph(m_icons.ObjectIcon(object));
        if (!object.GetMissingTypeName().empty())
        {
            color = EditorColorVec4(EditorColor::Missing);
            iconColor = color;
            icon = IconGlyph(Icon::TriangleAlert);
        }
        else if (object.IsLinked() && !context.GetLevelObjectEditService().IsEditingLevelObject())
        {
            color = EditorColorVec4(EditorColor::Linked);
            iconColor = color;
            icon = IconGlyph(Icon::Package);
        }
        if (!object.IsActiveInHierarchy())
        {
            color.w *= 0.5f;
            iconColor.w *= 0.5f;
        }
        bool keeping = context.GetLevelService().IsPlaying() && m_application.GetPlaySession() && m_application.GetPlaySession()->IsKeepingChanges(object.GetId());

        DrawRowBackground(context.GetSelectionService().IsSelected(object.GetId()));
        ImVec2 rowPosition = ImGui::GetCursorScreenPos();
        ImVec2 labelPosition(rowPosition.x + ImGui::GetTreeNodeToLabelSpacing(), rowPosition.y + ImGui::GetStyle().FramePadding.y);
        bool open;
        if (m_renaming == object.GetId())
        {
            open = ImGui::TreeNodeEx("##node", flags, "%s", "");
            ImGui::SameLine();
            ImGui::SetCursorScreenPos(ImVec2(labelPosition.x, rowPosition.y));
            if (m_focusRename)
            {
                ImGui::SetKeyboardFocusHere();
                m_focusRename = false;
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##rename", &m_renameBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
            {
                Serialization::JsonDocument before;
                ObjectEditService::Snapshot(object, before);
                object.SetName(m_renameBuffer);
                context.GetUndoService().RecordObjectChange(object.GetId(), before, Text(EditorText::Rename));
                m_renaming = {};
            }
            else if (!ImGui::IsItemActive() && !m_focusRename && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Escape)))
            {
                m_renaming = {};
            }
        }
        else
        {
            std::string suffix;
            if (!object.GetMissingTypeName().empty())
            {
                suffix = Text(EditorText::MissingTypeSuffix, object.GetMissingTypeName());
            }
            else if (&object.GetType() != &Level::LevelObject::StaticType())
            {
                suffix = "[" + object.GetType().GetName() + "]";
            }
            if (keeping)
            {
                suffix += (suffix.empty() ? "" : "  ") + std::string(Text(EditorText::KeepChangesSuffix));
            }
            open = ImGui::TreeNodeEx("##node", flags, "%s", "");
            DrawRowLabel(labelPosition, icon, iconColor, object.GetName(), color, suffix);
        }

        if (m_scrollTo == object.GetId())
        {
            ImGui::SetScrollHereY();
            m_scrollTo = {};
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        {
            context.GetSelectionService().Select(object.GetId(), ImGui::GetIO().KeyShift || ImGui::GetIO().KeySuper || ImGui::GetIO().KeyCtrl);
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            m_application.GetActiveLevelView().FocusSelection();
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
                Level::LevelObject* draggedObject = context.GetLevelService().GetActiveLevel()->FindObject(dragged);
                if (draggedObject && draggedObject != &object && !object.IsDescendantOf(draggedObject))
                {
                    if (ImGui::GetIO().KeyAlt)
                    {
                        context.GetObjectEditService().Reparent(*draggedObject, object.GetParent(), object.GetCollection(), object.GetSiblingIndex());
                    }
                    else
                    {
                        context.GetObjectEditService().Reparent(*draggedObject, &object, object.GetCollection(), -1);
                    }
                }
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRIGHT_ASSET"))
            {
                Core::Guid guid = *static_cast<const Core::Guid*>(payload->Data);
                const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(guid);
                if (record && record->type == Assets::AssetType::LevelObjectVariant)
                {
                    context.GetObjectEditService().InstantiateLevelObject(guid, object.GetTransform().GetPosition(), &object);
                }
                else if (record && record->type == Assets::AssetType::Model)
                {
                    context.GetObjectEditService().CreateModelObject(guid, record->name.substr(0, record->name.find('.')), object.GetTransform().GetPosition(), &object);
                }
                else if (record && record->type == Assets::AssetType::LevelObjectTemplate)
                {
                    Core::Log::Warning(Text(EditorText::CreateVariantFirst));
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
            std::vector<Level::LevelObject*> children = object.GetChildren();
            for (Level::LevelObject* child : children)
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
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
        bool selected = context.GetSelectionService().GetFocus() == SelectionFocus::Collection && context.GetSelectionService().GetSelectedCollection() == &collection;
        if (selected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        std::string label = collection.GetName();
        if (collection.IsDirty())
        {
            label += " *";
        }
        std::string suffix;
        if (context.GetLevelService().GetActiveCollection() == &collection && context.GetLevelService().GetEditLevel() && context.GetLevelService().GetEditLevel()->GetCollections().size() > 1)
        {
            suffix = Text(EditorText::ActiveSuffix);
        }
        DrawRowBackground(selected);
        ImVec2 rowPosition = ImGui::GetCursorScreenPos();
        ImVec2 labelPosition(rowPosition.x + ImGui::GetTreeNodeToLabelSpacing(), rowPosition.y + ImGui::GetStyle().FramePadding.y);
        bool open = ImGui::TreeNodeEx("##collection", flags, "%s", "");
        DrawRowLabel(labelPosition, IconGlyph(Icon::Layers), EditorColorVec4(EditorColor::TextDim), label, EditorColorVec4(EditorColor::Text), suffix);
        DrawRowTag(labelPosition.y, Text(EditorText::CollectionTag));
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            context.GetSelectionService().SelectCollection(&collection);
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRIGHT_OBJECT"))
            {
                Core::Guid dragged = *static_cast<const Core::Guid*>(payload->Data);
                if (Level::LevelObject* draggedObject = context.GetLevelService().GetActiveLevel()->FindObject(dragged))
                {
                    context.GetObjectEditService().Reparent(*draggedObject, nullptr, &collection, -1);
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("CollectionContext"))
        {
            if (ImGui::MenuItem(Text(EditorText::SetActiveCollection)))
            {
                context.GetLevelService().SetActiveCollection(&collection);
            }
            if (ImGui::BeginMenu(Text(EditorText::Create)))
            {
                context.GetLevelService().SetActiveCollection(&collection);
                DrawCreateMenu(nullptr);
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
        if (open)
        {
            std::vector<Level::LevelObject*> roots = collection.GetRoots();
            for (Level::LevelObject* root : roots)
            {
                DrawObject(*root);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void LevelContentsPanel::Draw()
    {
        if (!ImGui::Begin(WindowTitle(EditorWindow::LevelContents).c_str()))
        {
            ImGui::End();
            return;
        }
        EditorContext& context = m_application.GetContext();
        Level::Level* level = context.GetLevelService().GetActiveLevel();
        if (!level)
        {
            ImGui::TextDisabled("%s", Text(EditorText::NoLevelOpen));
            ImGui::End();
            return;
        }
        if (ImGui::Button(IconGlyph(Icon::Plus)))
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
        ImGui::InputTextWithHint("##filter", (std::string(IconGlyph(Icon::Search)) + "  Search").c_str(), &m_filter);

        std::string levelLabel = level->GetName() + (context.GetLevelService().IsDirty() ? " *" : "");
        std::string levelSuffix = context.GetLevelService().IsPlaying() ? Text(EditorText::PlayModeSuffix) : "";
        bool levelSelected = context.GetSelectionService().GetFocus() == SelectionFocus::Level;
        PushSelectionColors();
        ImGui::Spacing();
        ImVec2 levelPosition = ImGui::GetCursorScreenPos();
        float levelHeight = ImGui::GetFrameHeight();
        if (ImGui::Selectable("##level", levelSelected, ImGuiSelectableFlags_None, ImVec2(0, levelHeight)))
        {
            context.GetSelectionService().SelectLevel();
        }
        ImVec2 levelLabelPosition(levelPosition.x + ImGui::GetStyle().FramePadding.x, levelPosition.y + (levelHeight - ImGui::GetTextLineHeight()) * 0.5f);
        DrawRowLabel(levelLabelPosition, IconGlyph(Icon::Map), EditorColorVec4(EditorColor::Accent), levelLabel, EditorColorVec4(EditorColor::Text), levelSuffix);
        DrawRowTag(levelLabelPosition.y, Text(EditorText::AssetTypeLevel));
        ImGui::Spacing();
        ImGui::Separator();

        ImGui::BeginChild("Tree", ImVec2(0, 0), ImGuiChildFlags_None);
        m_rowIndex = 0;
        for (Level::LevelObject* selected : context.GetSelectionService().GetSelectedObjects())
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
            if (context.GetLevelService().IsPlaying() && collection->GetName() == Level::Level::RuntimeCollectionName && collection->GetRoots().empty())
            {
                continue;
            }
            DrawCollection(*collection);
        }
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
        {
            context.GetSelectionService().ClearSelection();
        }
        if (ImGui::BeginPopupContextWindow("TreeContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawCreateMenu(nullptr);
            ImGui::EndPopup();
        }
        if (ImGui::IsWindowFocused())
        {
            CommandRegistry::Get().DispatchShortcuts(CommandScope::LevelContents);
        }
        ImGui::EndChild();
        PopSelectionColors();
        ImGui::End();
    }
}
