#include "Panels/Assets/AssetsDropZone.h"

#include "DragDrop.h"
#include "EditorApplication.h"
#include "Panels/Assets/AssetsContextMenu.h"

#include <imgui.h>

#include <algorithm>

namespace BrightBox3D::Editor::AssetsDropZone
{
    namespace
    {
        constexpr float MinDropZoneHeight = 40.0f;

        void AcceptObjectDrop(EditorApplication& application, const std::string& folder)
        {
            if (!ImGui::BeginDragDropTarget())
            {
                return;
            }
            EditorContext& context = application.GetContext();
            if (std::optional<Core::Guid> id = AcceptPayload(DragDropPayload::Object))
            {
                if (Level::Level* level = context.GetLevelService().GetActiveLevel(); level && !context.GetLevelService().IsPlaying())
                {
                    if (Level::LevelObject* object = level->FindObject(*id))
                    {
                        context.GetAssetCreationService().CreateVariantAsset(*object, folder);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    void Draw(EditorApplication& application, AssetsPanelState& state, const std::string& folder)
    {
        ImGui::SetCursorPos(ImVec2(0, std::max(ImGui::GetCursorPosY(), 0.0f)));
        ImGui::InvisibleButton("##dropzone", ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x), std::max(MinDropZoneHeight, ImGui::GetContentRegionAvail().y)));
        if (ImGui::BeginPopupContextItem("BackgroundContext"))
        {
            AssetsContextMenu::Draw(application, state, { {}, folder });
            ImGui::EndPopup();
        }
        AcceptObjectDrop(application, folder);
        if (ImGui::BeginPopupContextWindow("GapContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            AssetsContextMenu::Draw(application, state, { {}, folder });
            ImGui::EndPopup();
        }
    }
}
