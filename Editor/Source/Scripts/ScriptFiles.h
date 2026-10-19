#pragma once

#include "EditorText.h"

#include <filesystem>
#include <string>

namespace BrightBox3D::Editor
{
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
}
