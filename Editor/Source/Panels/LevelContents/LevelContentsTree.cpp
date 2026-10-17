#include "Panels/LevelContents/LevelContentsTree.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "Core/StringUtils.h"
#include "DragDrop.h"
#include "EditorApplication.h"
#include "EditorPlaySession.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "IconGlyphs.h"
#include "Panels/LevelContents/LevelContentsContextMenu.h"
#include "Panels/LevelViewPanel.h"

#include <algorithm>
#include <vector>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr float LabelIconGap = 2.0f;
        constexpr float SelectionBarWidth = 2.0f;
        constexpr float RowTagAlpha = 0.7f;
        constexpr float InactiveAlpha = 0.5f;
        constexpr const char* SuffixSeparator = "  ";

        void DrawRowLabel(const ImVec2& position, const char* icon, const ImVec4& iconColor, const std::string& name, const ImVec4& nameColor, const std::string& suffix)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float iconWidth = ImGui::CalcTextSize(icon).x;
            drawList->AddText(position, ImGui::ColorConvertFloat4ToU32(iconColor), icon);
            ImVec2 namePosition(position.x + iconWidth + ImGui::GetStyle().ItemInnerSpacing.x + LabelIconGap, position.y);
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
            ImGui::GetWindowDrawList()->AddText(ImVec2(x, y), EditorColorU32(EditorColor::TextDim, RowTagAlpha), tag);
        }

        ImVec2 TreeLabelPosition(const ImVec2& rowPosition)
        {
            return ImVec2(rowPosition.x + ImGui::GetTreeNodeToLabelSpacing(), rowPosition.y + ImGui::GetStyle().FramePadding.y);
        }

        std::string WithDirtyMarker(const std::string& name, bool dirty)
        {
            return dirty ? Text(EditorText::DirtyMarker, name) : name;
        }

        std::string StemOf(const std::string& name)
        {
            return name.substr(0, name.find('.'));
        }
    }

    LevelContentsTree::LevelContentsTree(EditorApplication& application) : m_application(application)
    {
    }

    void LevelContentsTree::BeginRename(const Level::LevelObject& object)
    {
        m_rename.Begin(object.GetId(), object.GetName());
    }

    void LevelContentsTree::DrawRowBackground(bool selected)
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
            drawList->AddRectFilled(min, ImVec2(min.x + SelectionBarWidth, max.y), EditorColorU32(EditorColor::Accent));
        }
        drawList->PopClipRect();
        ++m_rowIndex;
    }

    bool LevelContentsTree::MatchesFilter(const Level::LevelObject& object) const
    {
        if (m_filter.empty())
        {
            return true;
        }
        if (Core::ToLower(object.GetName()).find(Core::ToLower(m_filter)) != std::string::npos)
        {
            return true;
        }
        return std::any_of(object.GetChildren().begin(), object.GetChildren().end(), [this](const Level::LevelObject* child) { return MatchesFilter(*child); });
    }

    void LevelContentsTree::DrawLevelRow(Level::Level& level)
    {
        EditorContext& context = m_application.GetContext();
        std::string label = WithDirtyMarker(level.GetName(), context.GetLevelService().IsDirty());
        std::string suffix = context.GetLevelService().IsPlaying() ? Text(EditorText::PlayModeSuffix) : "";
        bool selected = context.GetSelectionService().GetFocus() == SelectionFocus::Level;
        ImVec2 position = ImGui::GetCursorScreenPos();
        float height = ImGui::GetFrameHeight();
        if (ImGui::Selectable("##level", selected, ImGuiSelectableFlags_None, ImVec2(0, height)))
        {
            context.GetSelectionService().SelectLevel();
        }
        ImVec2 labelPosition(position.x + ImGui::GetStyle().FramePadding.x, position.y + (height - ImGui::GetTextLineHeight()) * 0.5f);
        DrawRowLabel(labelPosition, IconGlyph(Icon::Map), EditorColorVec4(EditorColor::Accent), label, EditorColorVec4(EditorColor::Text), suffix);
        DrawRowTag(labelPosition.y, Assets::AssetTypeName(Assets::AssetType::Level));
    }

    void LevelContentsTree::TrackScrollTarget()
    {
        for (Level::LevelObject* selected : m_application.GetContext().GetSelectionService().GetSelectedObjects())
        {
            if (selected->GetId() != m_lastSelected)
            {
                m_lastSelected = selected->GetId();
                m_scrollTo = selected->GetId();
            }
        }
    }

    void LevelContentsTree::DrawCollections(Level::Level& level, const std::string& filter)
    {
        m_filter = filter;
        m_rowIndex = 0;
        TrackScrollTarget();
        bool playing = m_application.GetContext().GetLevelService().IsPlaying();
        for (const auto& collection : level.GetCollections())
        {
            if (playing && collection->GetName() == Level::Level::RuntimeCollectionName && collection->GetRoots().empty())
            {
                continue;
            }
            DrawCollection(*collection);
        }
    }

    void LevelContentsTree::HandleCollectionDrop(Level::LevelCollection& collection)
    {
        if (!ImGui::BeginDragDropTarget())
        {
            return;
        }
        EditorContext& context = m_application.GetContext();
        if (std::optional<Core::Guid> dragged = AcceptPayload(DragDropPayload::Object))
        {
            if (Level::LevelObject* draggedObject = context.GetLevelService().GetActiveLevel()->FindObject(*dragged))
            {
                context.GetObjectEditService().Reparent(*draggedObject, nullptr, &collection, std::nullopt);
            }
        }
        ImGui::EndDragDropTarget();
    }

    void LevelContentsTree::DrawCollection(Level::LevelCollection& collection)
    {
        EditorContext& context = m_application.GetContext();
        ImGui::PushID(&collection);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
        bool selected = context.GetSelectionService().GetFocus() == SelectionFocus::Collection && context.GetSelectionService().GetSelectedCollection() == &collection;
        if (selected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        std::string label = WithDirtyMarker(collection.GetName(), collection.IsDirty());
        std::string suffix;
        Level::Level* editLevel = context.GetLevelService().GetEditLevel();
        if (context.GetLevelService().GetActiveCollection() == &collection && editLevel && editLevel->GetCollections().size() > 1)
        {
            suffix = Text(EditorText::ActiveSuffix);
        }
        DrawRowBackground(selected);
        ImVec2 labelPosition = TreeLabelPosition(ImGui::GetCursorScreenPos());
        bool open = ImGui::TreeNodeEx("##collection", flags, "%s", "");
        DrawRowLabel(labelPosition, IconGlyph(Icon::Layers), EditorColorVec4(EditorColor::TextDim), label, EditorColorVec4(EditorColor::Text), suffix);
        DrawRowTag(labelPosition.y, Assets::AssetTypeName(Assets::AssetType::LevelCollection));
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            context.GetSelectionService().SelectCollection(&collection);
        }
        HandleCollectionDrop(collection);
        if (ImGui::BeginPopupContextItem("CollectionContext"))
        {
            LevelContentsContextMenu::DrawCollectionMenu(m_application, collection);
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

    void LevelContentsTree::OpenTowardScrollTarget(Level::LevelObject& object)
    {
        EditorContext& context = m_application.GetContext();
        for (const Core::Guid& selected : context.GetSelectionService().GetSelection())
        {
            Level::LevelObject* selectedObject = context.GetLevelService().GetActiveLevel()->FindObject(selected);
            if (selectedObject && selectedObject->IsDescendantOf(&object) && m_scrollTo == selected)
            {
                ImGui::SetNextItemOpen(true);
            }
        }
    }

    LevelContentsTree::RowStyle LevelContentsTree::StyleFor(Level::LevelObject& object)
    {
        RowStyle style{ IconGlyph(m_icons.ObjectIcon(object)), EditorColorVec4(EditorColor::TextDim), EditorColorVec4(EditorColor::Text) };
        if (!object.GetMissingTypeName().empty())
        {
            style.nameColor = EditorColorVec4(EditorColor::Missing);
            style.iconColor = style.nameColor;
            style.icon = IconGlyph(Icon::TriangleAlert);
        }
        else if (object.IsLinked() && !m_application.GetContext().GetLevelObjectEditService().IsEditingLevelObject())
        {
            style.nameColor = EditorColorVec4(EditorColor::Linked);
            style.iconColor = style.nameColor;
            style.icon = IconGlyph(Icon::Package);
        }
        if (!object.IsActiveInHierarchy())
        {
            style.nameColor.w *= InactiveAlpha;
            style.iconColor.w *= InactiveAlpha;
        }
        return style;
    }

    std::string LevelContentsTree::SuffixFor(const Level::LevelObject& object) const
    {
        std::string suffix;
        if (!object.GetMissingTypeName().empty())
        {
            suffix = Text(EditorText::MissingTypeSuffix, object.GetMissingTypeName());
        }
        else if (&object.GetType() != &Level::LevelObject::StaticType())
        {
            suffix = Text(EditorText::TypeNameSuffix, object.GetType().GetName());
        }
        EditorPlaySession* session = m_application.GetPlaySession();
        if (m_application.GetContext().GetLevelService().IsPlaying() && session && session->IsKeepingChanges(object.GetId()))
        {
            suffix += (suffix.empty() ? "" : SuffixSeparator) + std::string(Text(EditorText::KeepChangesSuffix));
        }
        return suffix;
    }

    bool LevelContentsTree::DrawObjectRow(Level::LevelObject& object, ImGuiTreeNodeFlags flags)
    {
        RowStyle style = StyleFor(object);
        DrawRowBackground(m_application.GetContext().GetSelectionService().IsSelected(object.GetId()));
        ImVec2 rowPosition = ImGui::GetCursorScreenPos();
        ImVec2 labelPosition = TreeLabelPosition(rowPosition);
        if (!m_rename.IsRenaming(object.GetId()))
        {
            std::string suffix = SuffixFor(object);
            bool open = ImGui::TreeNodeEx("##node", flags, "%s", "");
            DrawRowLabel(labelPosition, style.icon, style.iconColor, object.GetName(), style.nameColor, suffix);
            return open;
        }
        bool open = ImGui::TreeNodeEx("##node", flags, "%s", "");
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(labelPosition.x, rowPosition.y));
        if (std::optional<std::string> renamed = m_rename.Draw(-FLT_MIN))
        {
            Serialization::JsonDocument before;
            ObjectEditService::Snapshot(object, before);
            object.SetName(*renamed);
            m_application.GetContext().GetUndoService().RecordObjectChange(object.GetId(), before, Text(EditorText::Rename));
        }
        return open;
    }

    void LevelContentsTree::HandleObjectInput(Level::LevelObject& object)
    {
        if (m_scrollTo == object.GetId())
        {
            ImGui::SetScrollHereY();
            m_scrollTo = {};
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        {
            m_application.GetContext().GetSelectionService().Select(object.GetId(), ImGui::GetIO().KeyShift || ImGui::GetIO().KeySuper || ImGui::GetIO().KeyCtrl ? SelectionMode::Toggle : SelectionMode::Replace);
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (LevelViewPanel* view = m_application.GetActiveLevelView())
            {
                view->FocusSelection();
            }
        }
        if (ImGui::BeginDragDropSource())
        {
            SetPayload(object.GetId(), DragDropPayload::Object);
            ImGui::TextUnformatted(object.GetName().c_str());
            ImGui::EndDragDropSource();
        }
    }

    void LevelContentsTree::HandleObjectDrop(Level::LevelObject& object)
    {
        if (!ImGui::BeginDragDropTarget())
        {
            return;
        }
        EditorContext& context = m_application.GetContext();
        if (std::optional<Core::Guid> dragged = AcceptPayload(DragDropPayload::Object))
        {
            Level::LevelObject* draggedObject = context.GetLevelService().GetActiveLevel()->FindObject(*dragged);
            if (draggedObject && draggedObject != &object && !object.IsDescendantOf(draggedObject))
            {
                if (ImGui::GetIO().KeyAlt)
                {
                    context.GetObjectEditService().Reparent(*draggedObject, object.GetParent(), object.GetCollection(), object.GetSiblingIndex());
                }
                else
                {
                    context.GetObjectEditService().Reparent(*draggedObject, &object, object.GetCollection(), std::nullopt);
                }
            }
        }
        if (std::optional<Core::Guid> dropped = AcceptPayload(DragDropPayload::Asset))
        {
            const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(*dropped);
            Math::Vector3 position = object.GetTransform().GetPosition();
            if (record && record->type == Assets::AssetType::LevelObjectVariant)
            {
                context.GetObjectEditService().InstantiateLevelObject(*dropped, position, &object);
            }
            else if (record && record->type == Assets::AssetType::Model)
            {
                context.GetObjectEditService().CreateModelObject(*dropped, StemOf(record->name), position, &object);
            }
            else if (record && record->type == Assets::AssetType::LevelObjectTemplate)
            {
                Core::Log::Warning(Text(EditorText::CreateVariantFirst));
            }
        }
        ImGui::EndDragDropTarget();
    }

    void LevelContentsTree::DrawObject(Level::LevelObject& object)
    {
        if (object.GetHideInLevelContents() || object.IsPendingDestroy() || !MatchesFilter(object))
        {
            return;
        }
        ImGui::PushID(object.GetId().ToString().c_str());
        bool hasChildren = std::any_of(object.GetChildren().begin(), object.GetChildren().end(), [](const Level::LevelObject* child) { return !child->GetHideInLevelContents(); });
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
        if (!hasChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (m_application.GetContext().GetSelectionService().IsSelected(object.GetId()))
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!m_filter.empty())
        {
            ImGui::SetNextItemOpen(true);
        }
        OpenTowardScrollTarget(object);

        bool open = DrawObjectRow(object, flags);
        HandleObjectInput(object);
        HandleObjectDrop(object);
        if (ImGui::BeginPopupContextItem("ObjectContext"))
        {
            LevelContentsContextMenu::DrawObjectMenu(m_application, object);
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
}
