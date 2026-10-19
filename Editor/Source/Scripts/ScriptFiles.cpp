#include "Scripts/ScriptFiles.h"

#include "Assets/AssetType.h"
#include "Services/ScriptLibrary.h"

#include <system_error>

namespace BrightBox3D::Editor::ScriptFiles
{
    namespace
    {
        Core::Result<void, EditorError> MovePath(const std::filesystem::path& from, const std::filesystem::path& to)
        {
            std::error_code error;
            if (std::filesystem::exists(to, error))
            {
                return EditorError(EditorText::ScriptPathExists, to.filename().string());
            }
            std::filesystem::create_directories(to.parent_path(), error);
            std::filesystem::rename(from, to, error);
            if (error)
            {
                return EditorError(EditorText::ScriptFileFailed, from.filename().string(), error.message());
            }
            return {};
        }

        bool IsInside(const std::string& folder, const std::string& candidate)
        {
            return candidate == folder || candidate.starts_with(folder + "/");
        }
    }

    std::string JoinFolder(const std::string& parent, const std::string& name)
    {
        return parent.empty() ? name : parent + "/" + name;
    }

    std::string FolderName(const std::string& folder)
    {
        return std::filesystem::path(folder).filename().string();
    }

    std::string ParentFolder(const std::string& folder)
    {
        return std::filesystem::path(folder).parent_path().generic_string();
    }

    Core::Result<std::string, EditorError> CreateFolder(const std::filesystem::path& root, const std::string& parent)
    {
        std::string base = Assets::NewAssetName(Assets::AssetType::Folder);
        std::string folder = JoinFolder(parent, base);
        std::error_code error;
        for (int suffix = 1; std::filesystem::exists(root / folder, error); ++suffix)
        {
            folder = JoinFolder(parent, base + " " + std::to_string(suffix));
        }
        std::filesystem::create_directories(root / folder, error);
        if (error)
        {
            return EditorError(EditorText::ScriptFileFailed, folder, error.message());
        }
        return folder;
    }

    Core::Result<std::string, EditorError> RenameFolder(const std::filesystem::path& root, const std::string& folder, const std::string& name)
    {
        std::string target = JoinFolder(ParentFolder(folder), name);
        if (target == folder)
        {
            return target;
        }
        if (Core::Result<void, EditorError> moved = MovePath(root / folder, root / target); !moved)
        {
            return moved.GetError();
        }
        return target;
    }

    Core::Result<std::string, EditorError> MoveFolder(const std::filesystem::path& root, const std::string& folder, const std::string& targetParent)
    {
        std::string target = JoinFolder(targetParent, FolderName(folder));
        if (IsInside(folder, targetParent) || target == folder)
        {
            return folder;
        }
        if (Core::Result<void, EditorError> moved = MovePath(root / folder, root / target); !moved)
        {
            return moved.GetError();
        }
        return target;
    }

    Core::Result<void, EditorError> MoveScript(const std::filesystem::path& root, const ScriptEntry& script, const std::string& targetFolder)
    {
        if (script.folder == targetFolder)
        {
            return {};
        }
        for (const std::filesystem::path& file : { script.header, script.source })
        {
            if (file.empty())
            {
                continue;
            }
            if (Core::Result<void, EditorError> moved = MovePath(file, root / JoinFolder(targetFolder, file.filename().string())); !moved)
            {
                return moved;
            }
        }
        return {};
    }
}
