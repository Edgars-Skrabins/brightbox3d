#include "Panels/AssetsPanel.h"

#include "Assets/AssetDatabase.h"
#include "Commands/CommandRegistry.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
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
        constexpr const char* DeletePopupId = "DeleteAsset";
    }

    AssetsPanel::AssetsPanel(EditorApplication& application, int instanceIndex) : EditorPanel(EditorWindow::Assets, instanceIndex), m_application(application), m_items(application, m_state), m_tree(application, m_state)
    {
    }

    AssetsTarget AssetsPanel::GetCommandTarget() const
    {
        if (m_state.menuTarget)
        {
            return *m_state.menuTarget;
        }
        SelectionService& selection = m_application.GetContext().GetSelectionService();
        if (selection.GetFocus() == SelectionFocus::Asset)
        {
            if (const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(selection.GetSelectedAsset()))
            {
                return TargetForAsset(*record);
            }
        }
        return { {}, m_state.folder };
    }

    void AssetsPanel::ImportFiles(const std::vector<std::filesystem::path>& files)
    {
        AssetFileImport::ImportFiles(files, m_state.folder);
    }

    void AssetsPanel::DrawDeleteConfirmation()
    {
        std::string title = LabelWithId(EditorText::DeleteAssetQuestion, DeletePopupId);
        if (m_state.pendingDelete.IsValid())
        {
            ImGui::OpenPopup(title.c_str());
        }
        if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        SelectionService& selection = m_application.GetContext().GetSelectionService();
        const Assets::AssetRecord* record = database.FindRecord(m_state.pendingDelete);
        ImGui::TextUnformatted(Text(EditorText::DeleteAssetConfirm, record ? record->path : std::string()).c_str());
        if (ImGui::Button(Text(EditorText::Delete), ImVec2(ScaledMetric(EditorMetric::DialogButtonWidth), 0)))
        {
            database.DeleteAsset(m_state.pendingDelete);
            if (selection.GetSelectedAsset() == m_state.pendingDelete)
            {
                selection.SelectAsset({});
            }
            m_state.pendingDelete = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::Cancel), ImVec2(ScaledMetric(EditorMetric::DialogButtonWidth), 0)))
        {
            m_state.pendingDelete = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void AssetsPanel::DrawTwoColumns()
    {
        if (!ImGui::BeginTable("AssetsLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            return;
        }
        ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, FolderColumnWidth);
        ImGui::TableSetupColumn("Items", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("FolderTree");
        AssetsFolderTree::Draw(m_application, m_state);
        ImGui::EndChild();
        ImGui::TableSetColumnIndex(1);
        m_items.Draw();
        ImGui::EndTable();
    }

    void AssetsPanel::Draw()
    {
        if (!BeginWindow())
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
        m_state.menuTarget.reset();
        PushSelectionColors();
        if (EditorPreferences::Get().GetAssetsLayout() == AssetsLayout::OneColumn)
        {
            m_tree.Draw();
        }
        else
        {
            DrawTwoColumns();
        }
        DrawDeleteConfirmation();
        PopSelectionColors();
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        {
            m_application.SetActivePanel(*this);
            CommandRegistry::Get().DispatchShortcuts(CommandScope::Assets);
        }
        ImGui::End();
    }
}
