#include "NewLevelDialog.h"

#include "EditorText.h"
#include "Labels.h"
#include "Runtime/ProjectLayout.h"
#include "Services/LevelService.h"
#include "Theme.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr const char* PopupId = "NewLevel";
        constexpr const char* DefaultLevelName = "NewLevel";
    }

    void NewLevelDialog::Open()
    {
        m_openRequested = true;
        m_name = DefaultLevelName;
    }

    void NewLevelDialog::Draw(LevelService& levels)
    {
        std::string title = LabelWithId(EditorText::NewLevelTitle, PopupId);
        if (m_openRequested)
        {
            ImGui::OpenPopup(title.c_str());
            m_openRequested = false;
        }
        if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        ImGui::InputText(Text(EditorText::Name), &m_name);
        if (ImGui::Button(Text(EditorText::Create), ImVec2(ScaledMetric(EditorMetric::DialogButtonWidth), 0)) && !m_name.empty())
        {
            if (levels.IsDirty())
            {
                levels.SaveLevel();
            }
            levels.NewLevel(Runtime::ProjectLayout::LevelAssetPath(m_name));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::Cancel), ImVec2(ScaledMetric(EditorMetric::DialogButtonWidth), 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
