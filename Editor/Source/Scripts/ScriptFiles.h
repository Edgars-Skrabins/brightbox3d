#pragma once

#include "EditorText.h"
#include "Scripts/ScriptTemplate.h"

#include <filesystem>
#include <string>

namespace BrightBox3D::Editor
{
    class ScriptLibrary;
    struct ScriptEntry;
}

namespace BrightBox3D::Editor::ScriptFiles
{
    std::string JoinFolder(const std::string& parent, const std::string& name);
    std::string FolderName(const std::string& folder);
    std::string ParentFolder(const std::string& folder);
    Core::Result<std::string, EditorError> CreateFolder(const std::filesystem::path& root, const std::string& parent);
    Core::Result<std::string, EditorError> RenameFolder(const std::filesystem::path& root, const std::string& folder, const std::string& name);
    Core::Result<std::string, EditorError> MoveFolder(const std::filesystem::path& root, const std::string& folder, const std::string& targetParent);
    Core::Result<void, EditorError> MoveScript(const std::filesystem::path& root, const ScriptEntry& script, const std::string& targetFolder);
    bool IsClassName(const std::string& name);
    Core::Result<void, EditorError> ValidateNewClassName(const std::string& name, ScriptLibrary& library);
    Core::Result<std::filesystem::path, EditorError> CreateScript(const std::filesystem::path& root, const std::string& folder, const std::string& className, ScriptTemplate scriptTemplate);
}
