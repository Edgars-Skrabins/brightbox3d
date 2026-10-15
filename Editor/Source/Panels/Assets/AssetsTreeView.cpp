#include "Panels/Assets/AssetsTreeView.h"

#include "Assets/AssetDatabase.h"
#include "Core/StringUtils.h"
#include "DragDrop.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "Panels/Assets/AssetActions.h"
#include "Panels/Assets/AssetsBottomBar.h"
#include "Panels/Assets/AssetsContextMenu.h"
#include "Panels/Assets/AssetsDropZone.h"
#include "Panels/Assets/AssetsFolderTree.h"
#include "Panels/Assets/AssetsToolbar.h"
#include "Panels/Assets/AssetTypeStyle.h"

#include <algorithm>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr Assets::AssetType BuiltinTypes[] = { Assets::AssetType::Model, Assets::AssetType::Material };
    }

    AssetsTreeView::AssetsTreeView(EditorApplication& application, AssetsPanelState& state) : m_application(application), m_state(state)
    {
    }

    std::string AssetsTreeView::CurrentFolder() const
    {
        return m_state.folder;
    }

    bool AssetsTreeView::IsSelected(const Core::Guid& guid) const
    {
        SelectionService& selection = m_application.GetContext().GetSelectionService();
        return selection.GetFocus() == SelectionFocus::Asset && selection.GetSelectedAsset() == guid;
    }

    std::vector<Assets::AssetRecord> AssetsTreeView::Children(const std::string& folder) const
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        std::vector<Assets::AssetRecord> children;
        for (const Assets::AssetRecord& record : database.GetRecords())
        {
            if (!database.IsBuiltin(record.guid) && ParentFolder(record.path) == folder)
            {
                children.push_back(record);
            }
        }
        std::stable_sort(children.begin(), children.end(), FoldersFirst);
        return children;
    }

    void AssetsTreeView::DrawToolbar()
    {
        AssetsToolbar::DrawCreateButton(m_application, CurrentFolder());
        ImGui::SameLine();
        AssetsToolbar::DrawSearch(m_state);
        ImGui::SameLine();
        AssetsToolbar::DrawPanelMenu();
    }

    bool AssetsTreeView::DrawNode(const Assets::AssetRecord* record, const std::string& id, const std::string& label, Assets::AssetType type, ImGuiTreeNodeFlags flags)
    {
        ImVec2 rowPosition = ImGui::GetCursorScreenPos();
        ImVec2 labelPosition(rowPosition.x + ImGui::GetTreeNodeToLabelSpacing(), rowPosition.y + ImGui::GetStyle().FramePadding.y);
        if (record && IsSelected(record->guid))
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        bool open = ImGui::TreeNodeEx(id.c_str(), flags, "%s", "");
        if (record && m_state.rename.IsRenaming(record->guid))
        {
            ImGui::SameLine();
            ImGui::SetCursorScreenPos(ImVec2(labelPosition.x, rowPosition.y));
            if (std::optional<std::string> renamed = m_state.rename.Draw(-FLT_MIN))
            {
                AssetActions::CommitRename(*record, *renamed);
            }
            return open;
        }
        bool expanded = open && !(flags & ImGuiTreeNodeFlags_Leaf);
        DrawAssetLabel(labelPosition, type, label, expanded);
        return open;
    }

    void AssetsTreeView::HandleNodeInput(const Assets::AssetRecord* record, const std::string& folder)
    {
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            if (record)
            {
                m_application.GetContext().GetSelectionService().SelectAsset(record->guid);
            }
            m_state.folder = folder;
            if (record && record->type != Assets::AssetType::Folder && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                AssetActions::Open(m_application, m_state, record->guid);
            }
        }
        if (record && !Assets::AssetDatabase::Get().IsBuiltin(record->guid) && ImGui::BeginDragDropSource())
        {
            SetPayload(record->guid, DragDropPayload::Asset);
            ImGui::TextUnformatted(record->name.c_str());
            ImGui::EndDragDropSource();
        }
        if (!record || record->type == Assets::AssetType::Folder)
        {
            AssetActions::AcceptMoveDrop(folder);
        }
        if (ImGui::BeginPopupContextItem("NodeContext"))
        {
            if (record)
            {
                m_application.GetContext().GetSelectionService().SelectAsset(record->guid);
            }
            AssetsContextMenu::Draw(m_application, m_state, record ? TargetForAsset(*record) : AssetsTarget{ {}, folder });
            ImGui::EndPopup();
        }
    }

    void AssetsTreeView::DrawAsset(const Assets::AssetRecord& record)
    {
        ImGui::PushID(record.guid.ToString().c_str());
        DrawNode(&record, "##asset", record.name, record.type, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        HandleNodeInput(&record, Assets::AssetDatabase::Get().IsBuiltin(record.guid) ? Assets::AssetDatabase::BuiltinFolder : ParentFolder(record.path));
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        {
            ImGui::SetTooltip("%s\n%s", record.path.c_str(), Assets::AssetTypeName(record.type));
        }
        ImGui::PopID();
    }

    void AssetsTreeView::DrawFolder(const std::string& folder, const std::string& label, const Assets::AssetRecord* record)
    {
        std::vector<Assets::AssetRecord> children = Children(folder);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (children.empty())
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (!record)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        bool open = DrawNode(record, record ? "##" + record->guid.ToString() : "##root", label, Assets::AssetType::Folder, flags);
        HandleNodeInput(record, folder);
        if (!open)
        {
            return;
        }
        for (const Assets::AssetRecord& child : children)
        {
            if (child.type == Assets::AssetType::Folder)
            {
                DrawFolder(child.path, child.name, &child);
            }
            else
            {
                DrawAsset(child);
            }
        }
        ImGui::TreePop();
    }

    void AssetsTreeView::DrawBuiltin()
    {
        bool open = DrawNode(nullptr, "##builtin", Text(EditorText::BuiltinAssets), Assets::AssetType::Folder, ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            m_state.folder = Assets::AssetDatabase::BuiltinFolder;
        }
        if (ImGui::BeginPopupContextItem("BuiltinContext"))
        {
            AssetsContextMenu::Draw(m_application, m_state, { {}, Assets::AssetDatabase::BuiltinFolder });
            ImGui::EndPopup();
        }
        if (!open)
        {
            return;
        }
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        for (Assets::AssetType type : BuiltinTypes)
        {
            for (const Assets::AssetRecord& record : database.GetRecords(type))
            {
                if (database.IsBuiltin(record.guid))
                {
                    DrawAsset(record);
                }
            }
        }
        ImGui::TreePop();
    }

    void AssetsTreeView::DrawSearchResults()
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        std::string filter = Core::ToLower(m_state.filter);
        std::vector<Assets::AssetRecord> matches;
        for (const Assets::AssetRecord& record : database.GetRecords())
        {
            if (!database.IsBuiltin(record.guid) && Core::ToLower(record.path).find(filter) != std::string::npos)
            {
                matches.push_back(record);
            }
        }
        std::stable_sort(matches.begin(), matches.end(), FoldersFirst);
        for (const Assets::AssetRecord& record : matches)
        {
            ImGui::PushID(record.guid.ToString().c_str());
            ImVec2 rowPosition = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;
            if (ImGui::Selectable("##match", IsSelected(record.guid), ImGuiSelectableFlags_AllowDoubleClick, ImVec2(width, ImGui::GetFrameHeight())))
            {
                m_application.GetContext().GetSelectionService().SelectAsset(record.guid);
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    AssetActions::Open(m_application, m_state, record.guid);
                }
            }
            HandleNodeInput(&record, record.type == Assets::AssetType::Folder ? record.path : ParentFolder(record.path));
            ImVec2 labelPosition(rowPosition.x + ImGui::GetStyle().FramePadding.x, rowPosition.y + ImGui::GetStyle().FramePadding.y);
            if (m_state.rename.IsRenaming(record.guid))
            {
                ImGui::SetCursorScreenPos(ImVec2(labelPosition.x, rowPosition.y));
                if (std::optional<std::string> renamed = m_state.rename.Draw(-FLT_MIN))
                {
                    AssetActions::CommitRename(record, *renamed);
                }
                ImGui::PopID();
                continue;
            }
            DrawAssetLabel(labelPosition, record.type, record.name, false);
            std::string path = AssetActions::DisplayPath(record);
            float pathX = rowPosition.x + width - ImGui::GetStyle().FramePadding.x - ImGui::CalcTextSize(path.c_str()).x;
            ImGui::GetWindowDrawList()->AddText(ImVec2(pathX, labelPosition.y), EditorColorU32(EditorColor::TextDim), path.c_str());
            ImGui::PopID();
        }
    }

    void AssetsTreeView::Draw()
    {
        DrawToolbar();
        ImGui::BeginChild("Tree", ImVec2(0, -AssetsBottomBar::Height()), ImGuiChildFlags_None);
        if (m_state.filter.empty())
        {
            DrawFolder("", Text(EditorText::AssetsTitle), nullptr);
            DrawBuiltin();
        }
        else
        {
            DrawSearchResults();
        }
        AssetsDropZone::Draw(m_application, m_state, CurrentFolder());
        ImGui::EndChild();
        AssetsBottomBar::Draw(m_application, false);
    }
}
