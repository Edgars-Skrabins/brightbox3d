#include "Panels/AssetsPanel.h"

#include "Assets/AssetDatabase.h"
#include "EditorApplication.h"
#include "EditorText.h"
#include "EditorWindows.h"
#include "Labels.h"
#include "Panels/Assets/AssetFileImport.h"
#include "Panels/Assets/AssetsFolderTree.h"
#include "Theme.h"

#include <imgui.h>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr float FolderColumnWidth = 180.0f;
        constexpr float DialogButtonWidth = 120.0f;
        constexpr const char* DeletePopupId = "DeleteAsset";
    }

    AssetsPanel::AssetsPanel(EditorApplication& application) : m_application(application), m_items(application)
    {
    }

    void AssetsPanel::ImportFiles(const std::vector<std::filesystem::path>& files)
    {
        AssetFileImport::ImportFiles(files, m_application.GetContext().GetAssetCreationService().GetAssetsFolder());
    }

    void AssetsPanel::DrawDeleteConfirmation()
    {
        std::string title = LabelWithId(EditorText::DeleteAssetQuestion, DeletePopupId);
        if (m_pendingDelete.IsValid())
        {
            ImGui::OpenPopup(title.c_str());
        }
        if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        SelectionService& selection = m_application.GetContext().GetSelectionService();
        const Assets::AssetRecord* record = database.FindRecord(m_pendingDelete);
        ImGui::TextUnformatted(Text(EditorText::DeleteAssetConfirm, record ? record->path : std::string(Text(EditorText::AssetBadgeUnknown))).c_str());
        if (ImGui::Button(Text(EditorText::Delete), ImVec2(DialogButtonWidth, 0)))
        {
            database.DeleteAsset(m_pendingDelete);
            if (selection.GetSelectedAsset() == m_pendingDelete)
            {
                selection.SelectAsset({});
            }
            m_pendingDelete = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::Cancel), ImVec2(DialogButtonWidth, 0)))
        {
            m_pendingDelete = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void AssetsPanel::Draw()
    {
        if (!ImGui::Begin(WindowTitle(EditorWindow::Assets).c_str()))
        {
            ImGui::End();
            return;
        }
        if (!Assets::AssetDatabase::Get().IsOpen())
        {
            ImGui::TextDisabled("%s", Text(EditorText::NoProjectOpen));
            ImGui::End();
            return;
        }
        PushSelectionColors();
        if (ImGui::BeginTable("AssetsLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, FolderColumnWidth);
            ImGui::TableSetupColumn("Items", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::BeginChild("FolderTree");
            AssetsFolderTree::Draw(m_application.GetContext());
            ImGui::EndChild();
            ImGui::TableSetColumnIndex(1);
            m_items.Draw(m_pendingDelete);
            ImGui::EndTable();
        }
        DrawDeleteConfirmation();
        PopSelectionColors();
        ImGui::End();
    }
}
