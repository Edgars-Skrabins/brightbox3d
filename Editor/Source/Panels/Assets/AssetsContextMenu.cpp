#include "Panels/Assets/AssetsContextMenu.h"

#include "Assets/AssetDatabase.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "EditorApplication.h"
#include "EditorText.h"
#include "Level/LevelSerializer.h"
#include "Rendering/MaterialAsset.h"
#include "Serialization/ObjectSerializer.h"
#include "Widgets/InlineRename.h"

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

        std::string RenameStem(const Assets::AssetRecord& record)
        {
            return record.type == Assets::AssetType::Folder ? record.name : record.name.substr(0, record.name.find('.'));
        }

        void Reimport(const Assets::AssetRecord& record)
        {
            Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
            std::string error;
            database.Unload(record.guid);
            if (database.CookAsset(record, nullptr, error))
            {
                database.OnAssetChanged.Broadcast(record.guid);
            }
            else
            {
                Core::Log::Error(Text(EditorText::ReimportFailed, error));
            }
        }
    }

    void OpenAsset(EditorApplication& application, const Core::Guid& guid)
    {
        EditorContext& context = application.GetContext();
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        const Assets::AssetRecord* record = database.FindRecord(guid);
        if (!record)
        {
            return;
        }
        switch (record->type)
        {
        case Assets::AssetType::Folder:
            context.GetAssetCreationService().SetAssetsFolder(record->path);
            break;
        case Assets::AssetType::Level:
            context.GetLevelService().OpenLevel(guid);
            break;
        case Assets::AssetType::LevelObjectVariant:
        case Assets::AssetType::LevelObjectTemplate:
            context.GetLevelObjectEditService().OpenLevelObjectEditor(guid);
            break;
        case Assets::AssetType::Screen:
        case Assets::AssetType::Widget:
        case Assets::AssetType::StyleSheet:
            application.OpenFileInEditor(database.GetAbsolutePath(*record).string(), 0);
            break;
        default:
            context.GetSelectionService().SelectAsset(guid);
            break;
        }
    }

    void DrawCreateMenu(EditorApplication& application, const std::string& folder)
    {
        EditorContext& context = application.GetContext();
        if (ImGui::MenuItem(Text(EditorText::AssetTypeFolder)))
        {
            CreateFolder(folder);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(Text(EditorText::AssetTypeLevel)))
        {
            context.GetSelectionService().SelectAsset(Level::LevelSerializer::CreateLevelAsset(NewAssetPath(folder, EditorText::NewLevelName, Assets::AssetType::Level)));
        }
        if (ImGui::MenuItem(Text(EditorText::AssetTypeLevelCollection)))
        {
            context.GetSelectionService().SelectAsset(CreateCollection(folder));
        }
        if (ImGui::MenuItem(Text(EditorText::AssetTypeLevelObjectTemplate)))
        {
            context.GetSelectionService().SelectAsset(context.GetAssetCreationService().CreateTemplateAsset(nullptr, folder));
        }
        if (ImGui::MenuItem(Text(EditorText::AssetTypeMaterial)))
        {
            context.GetSelectionService().SelectAsset(CreateMaterial(folder));
        }
        ImGui::Separator();
        if (ImGui::MenuItem(Text(EditorText::AssetTypeScreen)))
        {
            CreateTextAsset(folder, EditorText::NewScreenName, Assets::AssetType::Screen, DefaultScreenMarkup);
        }
        if (ImGui::MenuItem(Text(EditorText::AssetTypeWidget)))
        {
            CreateTextAsset(folder, EditorText::NewWidgetName, Assets::AssetType::Widget, DefaultWidgetMarkup);
        }
        if (ImGui::MenuItem(Text(EditorText::AssetTypeStyleSheet)))
        {
            CreateTextAsset(folder, EditorText::NewStyleSheetName, Assets::AssetType::StyleSheet, DefaultStyleSheet);
        }
    }

    void DrawItemMenu(EditorApplication& application, const Core::Guid& guid, InlineRename& rename, Core::Guid& pendingDelete)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        const Assets::AssetRecord* record = database.FindRecord(guid);
        if (!record)
        {
            return;
        }
        Runtime::Project& project = application.GetContext().GetProjectService().GetProject();
        bool builtin = database.IsBuiltin(guid);
        if (ImGui::MenuItem(Text(EditorText::Open)))
        {
            OpenAsset(application, guid);
        }
        ImGui::BeginDisabled(builtin);
        if (ImGui::MenuItem(Text(EditorText::Rename)))
        {
            rename.Begin(guid, RenameStem(*record));
        }
        if (ImGui::MenuItem(Text(EditorText::Delete)))
        {
            pendingDelete = guid;
        }
        ImGui::EndDisabled();
        if (record->type == Assets::AssetType::Level && ImGui::MenuItem(Text(EditorText::SetStartupLevel)))
        {
            project.SetStartupLevel(guid);
            project.Save();
        }
        if (record->type == Assets::AssetType::LevelObjectTemplate && ImGui::MenuItem(Text(EditorText::CreateVariant)))
        {
            application.GetContext().GetAssetCreationService().CreateVariantFromTemplate(guid);
        }
        ImGui::Separator();
        if (!builtin && ImGui::MenuItem(Text(EditorText::RevealInFileBrowser)))
        {
            application.RevealInFileBrowser(database.GetAbsolutePath(*record));
        }
        if (!builtin && record->type != Assets::AssetType::Folder && ImGui::MenuItem(Text(EditorText::Reimport)))
        {
            Reimport(*record);
        }
    }
}
