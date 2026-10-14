#include "Panels/Assets/AssetsFolderTree.h"

#include "Assets/AssetDatabase.h"
#include "DragDrop.h"
#include "EditorContext.h"
#include "EditorText.h"

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

        void DrawFolder(EditorContext& context, const std::string& folder, const std::string& label)
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
            if (context.GetAssetCreationService().GetAssetsFolder() == folder)
            {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            bool open = ImGui::TreeNodeEx(folder.empty() ? "##root" : folder.c_str(), flags, "%s", label.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                context.GetAssetCreationService().SetAssetsFolder(folder);
            }
            HandleDrop(folder);
            if (open)
            {
                for (const Assets::AssetRecord& child : folders)
                {
                    DrawFolder(context, child.path, child.name);
                }
                ImGui::TreePop();
            }
        }
    }

    void Draw(EditorContext& context)
    {
        DrawFolder(context, "", Text(EditorText::AssetsTitle));
        bool showBuiltin = context.GetAssetCreationService().GetAssetsFolder() == BuiltinFolder;
        if (ImGui::Selectable(Text(EditorText::BuiltinAssets), showBuiltin))
        {
            context.GetAssetCreationService().SetAssetsFolder(BuiltinFolder);
        }
    }
}
