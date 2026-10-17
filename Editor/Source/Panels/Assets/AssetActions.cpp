#include "Panels/Assets/AssetActions.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "DragDrop.h"
#include "EditorApplication.h"
#include "EditorText.h"
#include "Panels/Assets/AssetsFolderTree.h"

#include <imgui.h>

#include <algorithm>

namespace BrightBox3D::Editor
{
    AssetsTarget TargetForAsset(const Assets::AssetRecord& record)
    {
        if (Assets::AssetDatabase::Get().IsBuiltin(record.guid))
        {
            return { record.guid, Assets::AssetDatabase::BuiltinFolder };
        }
        return { record.guid, record.type == Assets::AssetType::Folder ? record.path : ParentFolder(record.path) };
    }
}

namespace BrightBox3D::Editor::AssetActions
{
    namespace
    {
        const Assets::AssetRecord* FindTarget(const AssetsTarget& target)
        {
            return target.asset.IsValid() ? Assets::AssetDatabase::Get().FindRecord(target.asset) : nullptr;
        }

        std::string RenameStem(const Assets::AssetRecord& record)
        {
            return record.name.substr(0, record.name.size() - Assets::MatchedExtension(record.type, record.name).size());
        }

        std::string ProjectPath(const std::string& relativePath)
        {
            std::string root = Assets::AssetDatabase::AssetsFolder;
            return relativePath.empty() ? root : root + "/" + relativePath;
        }
    }

    void Open(EditorApplication& application, AssetsPanelState& state, const Core::Guid& guid)
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
            state.folder = record->path;
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

    bool IsEditable(const AssetsTarget& target)
    {
        return FindTarget(target) && !Assets::AssetDatabase::Get().IsBuiltin(target.asset);
    }

    bool IsAssetOfType(const AssetsTarget& target, Assets::AssetType type)
    {
        const Assets::AssetRecord* record = FindTarget(target);
        return record && record->type == type;
    }

    bool CanCreateIn(const std::string& folder)
    {
        return folder != Assets::AssetDatabase::BuiltinFolder;
    }

    void BeginRename(AssetsPanelState& state, const AssetsTarget& target)
    {
        if (const Assets::AssetRecord* record = FindTarget(target))
        {
            state.rename.Begin(record->guid, RenameStem(*record));
        }
    }

    void CommitRename(const Assets::AssetRecord& record, const std::string& name)
    {
        std::string extension(Assets::MatchedExtension(record.type, record.name));
        std::string parent = ParentFolder(record.path);
        std::string target = (parent.empty() ? "" : parent + "/") + name + extension;
        if (!Assets::AssetDatabase::Get().MoveAsset(record.guid, target))
        {
            Core::Log::Error(Text(EditorText::RenameFailed, target));
        }
    }

    void ShowInFileBrowser(EditorApplication& application, const AssetsTarget& target)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        if (const Assets::AssetRecord* record = FindTarget(target))
        {
            application.RevealInFileBrowser(database.GetAbsolutePath(*record));
            return;
        }
        application.RevealInFileBrowser(target.folder.empty() ? database.GetAssetsDirectory() : database.GetAssetsDirectory() / target.folder);
    }

    void Duplicate(EditorApplication& application, const AssetsTarget& target)
    {
        Core::Guid copy = Assets::AssetDatabase::Get().DuplicateAsset(target.asset);
        if (copy.IsValid())
        {
            application.GetContext().GetSelectionService().SelectAsset(copy);
        }
    }

    void CopyPath(const AssetsTarget& target)
    {
        const Assets::AssetRecord* record = FindTarget(target);
        ImGui::SetClipboardText((record ? DisplayPath(*record) : ProjectPath(target.folder)).c_str());
    }

    std::string DisplayPath(const Assets::AssetRecord& record)
    {
        return Assets::AssetDatabase::Get().IsBuiltin(record.guid) ? record.path : ProjectPath(record.path);
    }

    void AcceptMoveDrop(const std::string& folder)
    {
        if (!ImGui::BeginDragDropTarget())
        {
            return;
        }
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        if (std::optional<Core::Guid> dragged = AcceptPayload(DragDropPayload::Asset))
        {
            const Assets::AssetRecord* record = database.FindRecord(*dragged);
            if (record && !database.IsBuiltin(*dragged) && record->path != folder && ParentFolder(record->path) != folder)
            {
                database.MoveAsset(*dragged, (folder.empty() ? "" : folder + "/") + record->name);
            }
        }
        ImGui::EndDragDropTarget();
    }

    void Reimport(const AssetsTarget& target)
    {
        const Assets::AssetRecord* found = FindTarget(target);
        if (!found)
        {
            return;
        }
        Assets::AssetRecord record = *found;
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        database.Unload(record.guid);
        if (Core::Result<std::vector<uint8_t>> cooked = database.CookAsset(record))
        {
            database.OnAssetChanged.Broadcast(record.guid);
        }
        else
        {
            Core::Log::Error(Text(EditorText::ReimportFailed, cooked.GetError().GetText()));
        }
    }

    void SetStartupLevel(EditorApplication& application, const AssetsTarget& target)
    {
        Runtime::Project& project = application.GetContext().GetProjectService().GetProject();
        project.SetStartupLevel(target.asset);
        project.Save();
    }

    void CreateVariant(EditorApplication& application, const AssetsTarget& target)
    {
        application.GetContext().GetAssetCreationService().CreateVariantFromTemplate(target.asset);
    }
}
