#include "Panels/LevelContents/LevelContentsContextMenu.h"

#include "Commands/CommandWidgets.h"
#include "CreateMenuRegistry.h"
#include "EditorApplication.h"
#include "EditorPlaySession.h"
#include "EditorText.h"
#include "Labels.h"
#include "Level/LevelSerializer.h"
#include "Panels/AssetsPanel.h"

#include <imgui.h>

namespace BrightBox3D::Editor::LevelContentsContextMenu
{
    namespace
    {
        void DrawLinkedRootItems(EditorContext& context, Level::LevelObject& object)
        {
            if (ImGui::MenuItem(Text(EditorText::SelectLevelObjectAsset, TypeName<Level::LevelObject>()).c_str()))
            {
                context.GetSelectionService().SelectAsset(object.GetLink().asset);
            }
            if (ImGui::MenuItem(Text(EditorText::OpenLevelObject, TypeName<Level::LevelObject>()).c_str()))
            {
                context.GetLevelObjectEditService().OpenLevelObjectEditor(object.GetLink().asset);
                return;
            }
            if (ImGui::MenuItem(Text(EditorText::ApplyAllOverrides)))
            {
                Level::LevelSerializer::ApplyAllToAsset(object);
            }
            if (ImGui::MenuItem(Text(EditorText::RevertAllOverrides)))
            {
                Serialization::JsonDocument before;
                ObjectEditService::Snapshot(object, before);
                Core::Guid id = object.GetId();
                Level::LevelSerializer::RevertAll(object);
                context.GetUndoService().RecordObjectChange(id, before, Text(EditorText::UndoRevertOverrides));
                return;
            }
            if (ImGui::MenuItem(Text(EditorText::Unlink)))
            {
                Serialization::JsonDocument before;
                ObjectEditService::Snapshot(object, before);
                Level::LevelSerializer::Unlink(object);
                context.GetUndoService().RecordObjectChange(object.GetId(), before, Text(EditorText::Unlink));
            }
        }

        void DrawAssetCreationItems(EditorApplication& application, Level::LevelObject& object)
        {
            AssetCreationService& assets = application.GetContext().GetAssetCreationService();
            AssetsPanel* panel = application.GetAssets();
            std::string folder = panel ? panel->GetState().folder : std::string();
            if (ImGui::MenuItem(Text(EditorText::CreateNamedType, Assets::AssetTypeName(Assets::AssetType::LevelObjectVariant)).c_str()))
            {
                assets.CreateVariantAsset(object, folder);
            }
            if (ImGui::MenuItem(Text(EditorText::CreateNamedType, Assets::AssetTypeName(Assets::AssetType::LevelObjectTemplate)).c_str()))
            {
                assets.CreateTemplateAsset(&object, folder);
            }
        }

        void DrawKeepChangesItem(EditorPlaySession& session, Level::LevelObject& object)
        {
            ImGui::Separator();
            bool keeping = session.IsKeepingChanges(object.GetId());
            if (ImGui::MenuItem(Text(EditorText::KeepChanges), nullptr, keeping))
            {
                session.ToggleKeepChanges(object.GetId());
            }
        }
    }

    void DrawCreateMenu(EditorApplication& application, Level::LevelObject* parent)
    {
        Math::Vector3 spawn = parent ? parent->GetTransform().GetPosition() : application.GetSpawnPoint();
        CreateMenuRegistry::Get().Draw({ application.GetContext(), parent, spawn });
    }

    void DrawObjectMenu(EditorApplication& application, Level::LevelObject& object)
    {
        EditorContext& context = application.GetContext();
        if (!context.GetSelectionService().IsSelected(object.GetId()))
        {
            context.GetSelectionService().Select(object.GetId());
        }
        CommandMenuItem(CommandId::Rename);
        CommandMenuItem(CommandId::Duplicate);
        if (CommandMenuItem(CommandId::Delete))
        {
            return;
        }
        CommandMenuItem(CommandId::FocusSelection);
        ImGui::Separator();
        if (ImGui::BeginMenu(Text(EditorText::CreateChild)))
        {
            DrawCreateMenu(application, &object);
            ImGui::EndMenu();
        }
        if (context.GetLevelService().IsPlaying())
        {
            if (EditorPlaySession* session = application.GetPlaySession())
            {
                DrawKeepChangesItem(*session, object);
            }
            return;
        }
        ImGui::Separator();
        if (object.GetLink().isRoot)
        {
            DrawLinkedRootItems(context, object);
        }
        else if (!object.IsLinked() && !context.GetLevelObjectEditService().IsEditingLevelObject())
        {
            DrawAssetCreationItems(application, object);
        }
    }

    void DrawCollectionMenu(EditorApplication& application, Level::LevelCollection& collection)
    {
        LevelService& levels = application.GetContext().GetLevelService();
        if (ImGui::MenuItem(Text(EditorText::SetActiveCollection)))
        {
            levels.SetActiveCollection(&collection);
        }
        if (ImGui::BeginMenu(Text(EditorText::Create)))
        {
            levels.SetActiveCollection(&collection);
            DrawCreateMenu(application, nullptr);
            ImGui::EndMenu();
        }
    }
}
