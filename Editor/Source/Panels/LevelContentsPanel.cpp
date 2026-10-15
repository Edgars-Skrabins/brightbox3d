#include "Panels/LevelContentsPanel.h"

#include "Commands/CommandRegistry.h"
#include "EditorApplication.h"
#include "EditorText.h"
#include "EditorWindows.h"
#include "IconGlyphs.h"
#include "Labels.h"
#include "Panels/LevelContents/LevelContentsContextMenu.h"
#include "Theme.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace BrightBox3D::Editor
{
    LevelContentsPanel::LevelContentsPanel(EditorApplication& application, int instanceIndex) : EditorPanel(EditorWindow::LevelContents, instanceIndex), m_application(application), m_tree(application)
    {
    }

    void LevelContentsPanel::BeginRenameSelection()
    {
        if (Level::LevelObject* primary = m_application.GetContext().GetSelectionService().GetPrimarySelection())
        {
            m_tree.BeginRename(*primary);
        }
    }

    void LevelContentsPanel::DrawToolbar()
    {
        if (IconButton(Icon::Plus, EditorText::CreateLevelObjectTooltip))
        {
            ImGui::OpenPopup("CreatePopup");
        }
        if (ImGui::BeginPopup("CreatePopup"))
        {
            LevelContentsContextMenu::DrawCreateMenu(m_application, nullptr);
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##filter", IconLabel(Icon::Search, EditorText::SearchHint).c_str(), &m_filter);
    }

    void LevelContentsPanel::HandleTreeInput()
    {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
        {
            m_application.GetContext().GetSelectionService().ClearSelection();
        }
        if (ImGui::BeginPopupContextWindow("TreeContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            LevelContentsContextMenu::DrawCreateMenu(m_application, nullptr);
            ImGui::EndPopup();
        }
        if (ImGui::IsWindowFocused())
        {
            CommandRegistry::Get().DispatchShortcuts(CommandScope::LevelContents);
        }
    }

    void LevelContentsPanel::Draw()
    {
        if (!BeginWindow())
        {
            ImGui::End();
            return;
        }
        Level::Level* level = m_application.GetContext().GetLevelService().GetActiveLevel();
        if (!level)
        {
            ImGui::TextDisabled("%s", Text(EditorText::NoLevelOpen));
            ImGui::End();
            return;
        }
        DrawToolbar();
        PushSelectionColors();
        ImGui::Spacing();
        m_tree.DrawLevelRow(*level);
        ImGui::Spacing();
        ImGui::Separator();

        ImGui::BeginChild("Tree", ImVec2(0, 0), ImGuiChildFlags_None);
        m_tree.DrawCollections(*level, m_filter);
        HandleTreeInput();
        ImGui::EndChild();
        PopSelectionColors();
        ImGui::End();
    }
}
