#include "Panels/Assets/AssetsFolderTree.h"

#include "Assets/AssetDatabase.h"
#include "EditorApplication.h"
#include "EditorText.h"
#include "Panels/Assets/AssetActions.h"
#include "Panels/Assets/AssetTypeStyle.h"
#include "Panels/Assets/AssetsContextMenu.h"
#include "Panels/Assets/EngineClassesView.h"
#include "Panels/Assets/ScriptsView.h"

#include <imgui.h>

#include <filesystem>
#include <vector>

namespace BrightBox3D::Editor
{
    std::string ParentFolder(const std::string& path)
    {
        return std::filesystem::path(path).parent_path().generic_string();
    }

    bool FoldersFirst(const Assets::AssetRecord& a, const Assets::AssetRecord& b)
    {
        bool aFolder = a.type == Assets::AssetType::Folder;
        bool bFolder = b.type == Assets::AssetType::Folder;
        if (aFolder != bFolder)
        {
            return aFolder;
        }
        return a.name < b.name;
    }
}

namespace BrightBox3D::Editor::AssetsFolderTree
{
    namespace
    {
        std::vector<Assets::AssetRecord> ChildFolders(const std::string& folder)
        {
            std::vector<Assets::AssetRecord> folders;
            for (const Assets::AssetRecord& record : Assets::AssetDatabase::Get().GetRecords(Assets::AssetType::Folder))
            {
                if (ParentFolder(record.path) == folder)
                {
                    folders.push_back(record);
                }
            }
            return folders;
        }

        AssetsTarget FolderTarget(const AssetsPanelState& state, const std::string& folder)
        {
            const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecordByPath(folder);
            if (!record || state.folder == folder)
            {
                return { {}, folder };
            }
            return TargetForAsset(*record);
        }

        bool DrawNode(AssetsPanelState& state, const Assets::AssetRecord* record, const std::string& folder, const std::string& label, ImGuiTreeNodeFlags flags)
        {
            ImVec2 rowPosition = ImGui::GetCursorScreenPos();
            ImVec2 labelPosition(rowPosition.x + ImGui::GetTreeNodeToLabelSpacing(), rowPosition.y + ImGui::GetStyle().FramePadding.y);
            bool open = ImGui::TreeNodeEx(folder.empty() ? "##root" : folder.c_str(), flags, "%s", "");
            if (!record || !state.rename.IsRenaming(record->guid))
            {
                DrawAssetLabel(labelPosition, Assets::AssetType::Folder, label, open && !(flags & ImGuiTreeNodeFlags_Leaf) ? FolderState::Open : FolderState::Closed);
                return open;
            }
            ImGui::SameLine();
            ImGui::SetCursorScreenPos(ImVec2(labelPosition.x, rowPosition.y));
            if (std::optional<std::string> renamed = state.rename.Draw(-FLT_MIN))
            {
                AssetActions::CommitRename(*record, *renamed);
            }
            return open;
        }

        void DrawFolder(EditorApplication& application, AssetsPanelState& state, const std::string& folder, const std::string& label)
        {
            std::vector<Assets::AssetRecord> folders = ChildFolders(folder);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (folders.empty())
            {
                flags |= ImGuiTreeNodeFlags_Leaf;
            }
            if (folder.empty())
            {
                flags |= ImGuiTreeNodeFlags_DefaultOpen;
            }
            if (state.root == AssetsRoot::Assets && state.folder == folder)
            {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            const Assets::AssetRecord* record = folder.empty() ? nullptr : Assets::AssetDatabase::Get().FindRecordByPath(folder);
            bool open = DrawNode(state, record, folder, label, flags);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                state.root = AssetsRoot::Assets;
                state.folder = folder;
            }
            if (ImGui::BeginPopupContextItem("FolderContext"))
            {
                AssetsContextMenu::Draw(application, state, FolderTarget(state, folder));
                ImGui::EndPopup();
            }
            AssetActions::AcceptMoveDrop(folder);
            if (open)
            {
                for (const Assets::AssetRecord& child : folders)
                {
                    DrawFolder(application, state, child.path, child.name);
                }
                ImGui::TreePop();
            }
        }
    }

    void Draw(EditorApplication& application, AssetsPanelState& state)
    {
        DrawFolder(application, state, "", Text(EditorText::AssetsTitle));
        ScriptsView::DrawFolderTree(application, state);
        bool showBuiltin = state.root == AssetsRoot::Assets && state.folder == Assets::AssetDatabase::BuiltinFolder;
        ImVec2 builtinPosition = ImGui::GetCursorScreenPos();
        if (ImGui::Selectable("##builtin", showBuiltin))
        {
            state.root = AssetsRoot::Assets;
            state.folder = Assets::AssetDatabase::BuiltinFolder;
        }
        DrawAssetLabel(builtinPosition, Assets::AssetType::Folder, Text(EditorText::BuiltinAssets), FolderState::Closed);
        if (ImGui::BeginPopupContextItem("BuiltinContext"))
        {
            AssetsContextMenu::Draw(application, state, { {}, Assets::AssetDatabase::BuiltinFolder });
            ImGui::EndPopup();
        }
        ImGui::Indent();
        EngineClassesView::DrawFolderTree(state);
        ImGui::Unindent();
    }
}
