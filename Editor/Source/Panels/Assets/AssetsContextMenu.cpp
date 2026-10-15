#include "Panels/Assets/AssetsContextMenu.h"

#include "Assets/AssetDatabase.h"
#include "Commands/CommandWidgets.h"
#include "Core/FileSystem.h"
#include "EditorApplication.h"
#include "EditorContent.h"
#include "EditorLayout.h"
#include "EditorText.h"
#include "Level/LevelSerializer.h"
#include "Panels/Assets/AssetActions.h"
#include "Rendering/MaterialAsset.h"
#include "Serialization/ObjectSerializer.h"

#include <imgui.h>

namespace BrightBox3D::Editor::AssetsContextMenu
{
    namespace
    {
        std::string NewAssetPath(const std::string& folder, Assets::AssetType type)
        {
            std::string prefix = folder.empty() ? "" : folder + "/";
            return prefix + Assets::NewAssetName(type) + Assets::AssetTypeExtension(type);
        }

        std::string StarterContent(Assets::AssetType type)
        {
            std::filesystem::path file = EditorContentDirectory() / EditorLayout::AssetTemplatesFolder / (std::string(Assets::AssetTypeName(type)) + Assets::AssetTypeExtension(type));
            return Core::FileSystem::ReadText(file).value_or(std::string());
        }

        void CreateTextAsset(const std::string& folder, Assets::AssetType type)
        {
            Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
            std::string path = database.MakeUniquePath(NewAssetPath(folder, type));
            Core::FileSystem::WriteText(database.GetAssetsDirectory() / path, StarterContent(type));
            database.Refresh();
        }

        void CreateFolder(const std::string& folder)
        {
            Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
            std::string path = database.MakeUniquePath(NewAssetPath(folder, Assets::AssetType::Folder));
            Core::FileSystem::EnsureDirectory(database.GetAssetsDirectory() / path);
            database.Refresh();
        }

        Core::Guid CreateCollection(const std::string& folder)
        {
            Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
            Serialization::JsonDocument document;
            Level::LevelSerializer::WriteEmptyCollection(Assets::NewAssetName(Assets::AssetType::LevelCollection), document, document.GetAllocator());
            return database.CreateJsonAsset(database.MakeUniquePath(NewAssetPath(folder, Assets::AssetType::LevelCollection)), document);
        }

        Core::Guid CreateMaterial(const std::string& folder)
        {
            Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
            Rendering::MaterialAsset material;
            Serialization::JsonDocument document;
            Serialization::SerializeProperties(material, document, document.GetAllocator());
            return database.CreateJsonAsset(database.MakeUniquePath(NewAssetPath(folder, Assets::AssetType::Material)), document);
        }
    }

    void DrawCreateMenu(EditorApplication& application, const std::string& folder)
    {
        EditorContext& context = application.GetContext();
        if (ImGui::MenuItem(Assets::AssetTypeName(Assets::AssetType::Folder)))
        {
            CreateFolder(folder);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(Assets::AssetTypeName(Assets::AssetType::Level)))
        {
            context.GetSelectionService().SelectAsset(Level::LevelSerializer::CreateLevelAsset(NewAssetPath(folder, Assets::AssetType::Level)));
        }
        if (ImGui::MenuItem(Assets::AssetTypeName(Assets::AssetType::LevelCollection)))
        {
            context.GetSelectionService().SelectAsset(CreateCollection(folder));
        }
        if (ImGui::MenuItem(Assets::AssetTypeName(Assets::AssetType::LevelObjectTemplate)))
        {
            context.GetSelectionService().SelectAsset(context.GetAssetCreationService().CreateTemplateAsset(nullptr, folder));
        }
        if (ImGui::MenuItem(Assets::AssetTypeName(Assets::AssetType::Material)))
        {
            context.GetSelectionService().SelectAsset(CreateMaterial(folder));
        }
        ImGui::Separator();
        if (ImGui::MenuItem(Text(EditorText::UITypeLabel, Assets::AssetTypeName(Assets::AssetType::Screen)).c_str()))
        {
            CreateTextAsset(folder, Assets::AssetType::Screen);
        }
        if (ImGui::MenuItem(Text(EditorText::UITypeLabel, Assets::AssetTypeName(Assets::AssetType::Widget)).c_str()))
        {
            CreateTextAsset(folder, Assets::AssetType::Widget);
        }
        if (ImGui::MenuItem(Text(EditorText::UITypeLabel, Assets::AssetTypeName(Assets::AssetType::StyleSheet)).c_str()))
        {
            CreateTextAsset(folder, Assets::AssetType::StyleSheet);
        }
    }

    void Draw(EditorApplication& application, AssetsPanelState& state, const AssetsTarget& target)
    {
        state.menuTarget = target;
        if (ImGui::BeginMenu(Text(EditorText::Create), AssetActions::CanCreateIn(target.folder)))
        {
            DrawCreateMenu(application, target.folder);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        CommandMenuItem(CommandId::AssetShowInFileBrowser);
        CommandMenuItem(CommandId::AssetOpen);
        CommandMenuItem(CommandId::AssetRename);
        CommandMenuItem(CommandId::AssetDelete);
        CommandMenuItem(CommandId::AssetDuplicate);
        CommandMenuItem(CommandId::AssetCopyPath);
        ImGui::Separator();
        CommandMenuItem(CommandId::AssetSetStartupLevel);
        CommandMenuItem(CommandId::AssetCreateVariant);
        ImGui::Separator();
        CommandMenuItem(CommandId::AssetReimport);
        CommandMenuItem(CommandId::AssetRefresh);
    }
}
