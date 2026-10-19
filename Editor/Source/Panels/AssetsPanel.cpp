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
#include "Panels/Assets/ScriptActions.h"
#include "Theme.h"

#include <imgui.h>

#include <algorithm>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr float FolderColumnWidth = 180.0f;
        constexpr const char* DeletePopupId = "DeleteAsset";
        constexpr const char* DeleteScriptPopupId = "DeleteScript";
        constexpr const char* FolderKey = "folder";
        constexpr const char* RootKey = "root";
        constexpr const char* ScriptFolderKey = "scriptFolder";
    }

    AssetsPanel::AssetsPanel(EditorApplication& application, int instanceIndex) : EditorPanel(EditorWindow::Assets, instanceIndex), m_application(application), m_items(application, m_state), m_tree(application, m_state)
    {
    }

    void AssetsPanel::SaveState(Serialization::JsonValue& state, Serialization::JsonAllocator& allocator) const
    {
        Serialization::SetMember(state, FolderKey, Serialization::MakeString(m_state.folder, allocator), allocator);
        Serialization::SetMember(state, RootKey, Serialization::JsonValue(static_cast<int>(m_state.root)), allocator);
        Serialization::SetMember(state, ScriptFolderKey, Serialization::MakeString(m_state.scriptFolder, allocator), allocator);
    }

    void AssetsPanel::LoadState(const Serialization::JsonValue& state)
    {
        m_state.folder = Serialization::GetString(state, FolderKey);
        m_state.root = static_cast<AssetsRoot>(std::clamp(static_cast<int>(Serialization::GetNumber(state, RootKey)), 0, static_cast<int>(AssetsRoot::EngineClasses)));
        m_state.scriptFolder = Serialization::GetString(state, ScriptFolderKey);
    }

    AssetsTarget AssetsPanel::GetCommandTarget() const
    {
        if (m_state.menuTarget)
        {
            return *m_state.menuTarget;
        }
        if (m_state.root == AssetsRoot::Scripts)
        {
            if (const ScriptEntry* script = m_application.GetContext().GetScriptLibrary().Find(m_state.selectedScript))
            {
                return ScriptActions::TargetForScript(*script);
            }
            return ScriptActions::TargetForFolder(m_state.scriptFolder);
        }
        if (m_state.root == AssetsRoot::EngineClasses)
        {
            return { {}, m_state.engineModule, AssetsRoot::EngineClasses, {} };
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

    void AssetsPanel::DrawScriptDeleteConfirmation()
    {
        std::string title = LabelWithId(EditorText::DeleteScriptQuestion, DeleteScriptPopupId);
        if (m_state.pendingScriptDelete.IsValid())
        {
            ImGui::OpenPopup(title.c_str());
        }
        if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        const ScriptEntry* script = m_application.GetContext().GetScriptLibrary().Find(m_state.pendingScriptDelete);
        ImGui::TextUnformatted(Text(EditorText::DeleteScriptConfirm, script ? script->className : std::string()).c_str());
        if (ImGui::Button(Text(EditorText::Delete), ImVec2(ScaledMetric(EditorMetric::DialogButtonWidth), 0)))
        {
            ScriptActions::DeleteScript(m_application, m_state, m_state.pendingScriptDelete);
            m_state.pendingScriptDelete = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::Cancel), ImVec2(ScaledMetric(EditorMetric::DialogButtonWidth), 0)))
        {
            m_state.pendingScriptDelete = {};
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
        if (!m_state.folder.empty() && m_state.folder != Assets::AssetDatabase::BuiltinFolder && !Assets::AssetDatabase::Get().FindRecordByPath(m_state.folder))
        {
            m_state.folder.clear();
        }
        if (!m_application.GetContext().GetScriptLibrary().HasFolder(m_state.scriptFolder))
        {
            m_state.scriptFolder.clear();
        }
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
        DrawScriptDeleteConfirmation();
        PopSelectionColors();
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        {
            m_application.SetActivePanel(*this);
            CommandRegistry::Get().DispatchShortcuts(CommandScope::Assets);
        }
        ImGui::End();
    }
}
