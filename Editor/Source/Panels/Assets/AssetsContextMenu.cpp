#include "Panels/Assets/AssetsContextMenu.h"

#include "Assets/AssetDatabase.h"
#include "Commands/CommandWidgets.h"
#include "Core/FileSystem.h"
#include "EditorApplication.h"
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
        constexpr const char* DefaultScreenMarkup = "<Screen>\n    <Panel class=\"root\">\n        <Label text=\"Hello BrightBox3D\" />\n    </Panel>\n</Screen>\n";
        constexpr const char* DefaultWidgetMarkup = "<Widget>\n    <Panel class=\"widget\">\n        <Label text=\"Widget\" />\n    </Panel>\n</Widget>\n";
        constexpr const char* DefaultStyleSheet = ".root {\n    width: 100%;\n    height: 100%;\n    padding: 16px;\n}\n";

        std::string NewAssetPath(const std::string& folder, EditorText name, Assets::AssetType type)
        {
            std::string prefix = folder.empty() ? "" : folder + "/";
            return prefix + Text(name) + Assets::AssetTypeExtension(type);
        }

        void CreateTextAsset(const std::string& folder, EditorText name, Assets::AssetType type, const char* content)
        {
            Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
            std::string path = database.MakeUniquePath(NewAssetPath(folder, name, type));
            Core::FileSystem::WriteText(database.GetAssetsDirectory() / path, content);
            database.Refresh();
        }

        void CreateFolder(const std::string& folder)
        {
            Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
            std::string path = database.MakeUniquePath(NewAssetPath(folder, EditorText::NewFolderName, Assets::AssetType::Folder));
            Core::FileSystem::EnsureDirectory(database.GetAssetsDirectory() / path);
            database.Refresh();
        }

        Core::Guid CreateCollection(const std::string& folder)
        {
            Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
            Serialization::JsonDocument document;
            Level::LevelSerializer::WriteEmptyCollection(Text(EditorText::NewCollectionName), document, document.GetAllocator());
            return database.CreateJsonAsset(database.MakeUniquePath(NewAssetPath(folder, EditorText::NewCollectionName, Assets::AssetType::LevelCollection)), document);
        }

        Core::Guid CreateMaterial(const std::string& folder)
        {
            Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
            Rendering::MaterialAsset material;
            Serialization::JsonDocument document;
            Serialization::SerializeProperties(material, document, document.GetAllocator());
            return database.CreateJsonAsset(database.MakeUniquePath(NewAssetPath(folder, EditorText::NewMaterialName, Assets::AssetType::Material)), document);
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
            context.GetSelectionService().SelectAsset(Level::LevelSerializer::CreateLevelAsset(NewAssetPath(folder, EditorText::NewLevelName, Assets::AssetType::Level)));
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
            CreateTextAsset(folder, EditorText::NewScreenName, Assets::AssetType::Screen, DefaultScreenMarkup);
        }
        if (ImGui::MenuItem(Text(EditorText::UITypeLabel, Assets::AssetTypeName(Assets::AssetType::Widget)).c_str()))
        {
            CreateTextAsset(folder, EditorText::NewWidgetName, Assets::AssetType::Widget, DefaultWidgetMarkup);
        }
        if (ImGui::MenuItem(Text(EditorText::UITypeLabel, Assets::AssetTypeName(Assets::AssetType::StyleSheet)).c_str()))
        {
            CreateTextAsset(folder, EditorText::NewStyleSheetName, Assets::AssetType::StyleSheet, DefaultStyleSheet);
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
