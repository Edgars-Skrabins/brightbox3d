#include "Panels/Assets/AssetsFolderTree.h"

#include "Assets/AssetDatabase.h"
#include "DragDrop.h"
#include "EditorApplication.h"
#include "EditorText.h"
#include "Panels/Assets/AssetActions.h"
#include "Panels/Assets/AssetsContextMenu.h"

#include <imgui.h>

#include <filesystem>
#include <vector>

namespace BrightBox3D::Editor
{
    std::string ParentFolder(const std::string& path)
    {
        return std::filesystem::path(path).parent_path().generic_string();
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

        void HandleDrop(const std::string& folder)
        {
            if (!ImGui::BeginDragDropTarget())
            {
                return;
            }
            if (std::optional<Core::Guid> dragged = AcceptPayload(DragDropPayload::Asset))
            {
                if (const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(*dragged))
                {
                    Assets::AssetDatabase::Get().MoveAsset(*dragged, (folder.empty() ? "" : folder + "/") + record->name);
                }
            }
            ImGui::EndDragDropTarget();
        }

        AssetsTarget FolderTarget(EditorContext& context, const std::string& folder)
        {
            const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecordByPath(folder);
            if (!record || context.GetAssetCreationService().GetAssetsFolder() == folder)
            {
                return { {}, folder };
            }
            return TargetForAsset(*record);
        }

        bool DrawNode(AssetsPanelState& state, const Assets::AssetRecord* record, const std::string& folder, const std::string& label, ImGuiTreeNodeFlags flags)
        {
            if (!record || !state.rename.IsRenaming(record->guid))
            {
                return ImGui::TreeNodeEx(folder.empty() ? "##root" : folder.c_str(), flags, "%s", label.c_str());
            }
            bool open = ImGui::TreeNodeEx(folder.c_str(), flags, "%s", "");
            ImGui::SameLine();
            if (std::optional<std::string> renamed = state.rename.Draw(-FLT_MIN))
            {
                AssetActions::CommitRename(*record, *renamed);
            }
            return open;
        }

        void DrawFolder(EditorApplication& application, AssetsPanelState& state, const std::string& folder, const std::string& label)
        {
            EditorContext& context = application.GetContext();
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
            if (context.GetAssetCreationService().GetAssetsFolder() == folder)
            {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            const Assets::AssetRecord* record = folder.empty() ? nullptr : Assets::AssetDatabase::Get().FindRecordByPath(folder);
            bool open = DrawNode(state, record, folder, label, flags);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                context.GetAssetCreationService().SetAssetsFolder(folder);
            }
            if (ImGui::BeginPopupContextItem("FolderContext"))
            {
                AssetsContextMenu::Draw(application, state, FolderTarget(context, folder));
                ImGui::EndPopup();
            }
            HandleDrop(folder);
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
        EditorContext& context = application.GetContext();
        DrawFolder(application, state, "", Text(EditorText::AssetsTitle));
        bool showBuiltin = context.GetAssetCreationService().GetAssetsFolder() == BuiltinFolder;
        if (ImGui::Selectable(Text(EditorText::BuiltinAssets), showBuiltin))
        {
            context.GetAssetCreationService().SetAssetsFolder(BuiltinFolder);
        }
        if (ImGui::BeginPopupContextItem("BuiltinContext"))
        {
            AssetsContextMenu::Draw(application, state, { {}, BuiltinFolder });
            ImGui::EndPopup();
        }
    }
}
