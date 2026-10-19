#include "Scripts/ScriptFiles.h"

#include "Assets/AssetType.h"
#include "Core/FileSystem.h"
#include "Platform/Shell.h"
#include "Reflection/TypeRegistry.h"
#include "Services/ScriptLibrary.h"

#include <cctype>
#include <regex>

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

    bool IsClassName(const std::string& name)
    {
        if (name.empty() || std::isdigit(static_cast<unsigned char>(name.front())))
        {
            return false;
        }
        for (char character : name)
        {
            if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
            {
                return false;
            }
        }
        return true;
    }

    Core::Result<void, EditorError> ValidateNewClassName(const std::string& name, ScriptLibrary& library)
    {
        if (!IsClassName(name))
        {
            return EditorError(EditorText::ScriptNameInvalid, name);
        }
        if (Reflection::TypeRegistry::Find(name) || library.FindByClass(name))
        {
            return EditorError(EditorText::ScriptNameTaken, name);
        }
        return {};
    }

    Core::Result<std::filesystem::path, EditorError> CreateScript(const std::filesystem::path& root, const std::string& folder, const std::string& className, ScriptTemplate scriptTemplate)
    {
        Core::Result<ScriptSources, EditorError> sources = RenderScriptTemplate(scriptTemplate, className);
        if (!sources)
        {
            return sources.GetError();
        }
        std::filesystem::path directory = folder.empty() ? root : root / folder;
        std::filesystem::path header = directory / (className + ScriptLibrary::HeaderExtension);
        std::filesystem::path source = directory / (className + ScriptLibrary::SourceExtension);
        std::error_code error;
        for (const std::filesystem::path& file : { header, source })
        {
            if (std::filesystem::exists(file, error))
            {
                return EditorError(EditorText::ScriptPathExists, file.filename().string());
            }
        }
        if (!Core::FileSystem::WriteText(header, sources->header) || !Core::FileSystem::WriteText(source, sources->source))
        {
            return EditorError(EditorText::ScriptWriteFailed, source.filename().string());
        }
        return source;
    }

    Core::Result<void, EditorError> RenameScript(const ScriptEntry& script, const std::string& newName, ScriptLibrary& library)
    {
        if (newName == script.className)
        {
            return {};
        }
        if (Core::Result<void, EditorError> valid = ValidateNewClassName(newName, library); !valid)
        {
            return valid;
        }
        std::regex word("\\b" + script.className + "\\b");
        for (const std::filesystem::path& file : { script.header, script.source })
        {
            if (file.empty())
            {
                continue;
            }
            std::filesystem::path target = file.parent_path() / (newName + file.extension().string());
            std::error_code error;
            if (std::filesystem::exists(target, error))
            {
                return EditorError(EditorText::ScriptPathExists, target.filename().string());
            }
            std::optional<std::string> text = Core::FileSystem::ReadText(file);
            if (!text)
            {
                return EditorError(EditorText::ScriptWriteFailed, file.filename().string());
            }
            std::string renamed = std::regex_replace(*text, word, newName);
            if (file == script.source)
            {
                Core::Result<std::string, EditorError> redirect = RenderTypeRedirect(newName, script.className);
                if (!redirect)
                {
                    return redirect.GetError();
                }
                renamed += *redirect;
            }
            if (!Core::FileSystem::WriteText(target, renamed))
            {
                return EditorError(EditorText::ScriptWriteFailed, target.filename().string());
            }
            std::filesystem::remove(file, error);
        }
        return {};
    }

    Core::Result<void, EditorError> DeleteScript(const ScriptEntry& script)
    {
        for (const std::filesystem::path& file : { script.header, script.source })
        {
            if (file.empty())
            {
                continue;
            }
            if (Core::Result<void> trashed = Platform::Shell::MoveToTrash(file); !trashed)
            {
                return EditorError(EditorText::ScriptFileFailed, file.filename().string(), trashed.GetError().GetText());
            }
        }
        return {};
    }
}
