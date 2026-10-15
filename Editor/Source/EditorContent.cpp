#include "EditorContent.h"

#include "Core/FileSystem.h"
#include "EditorLayout.h"

namespace BrightBox3D::Editor
{
    std::filesystem::path EditorContentDirectory()
    {
        return Core::FileSystem::FindContentDirectory(EditorLayout::ContentFolder, std::filesystem::path(EditorLayout::SourceTreeEditorFolder) / EditorLayout::SourceTreeContentFolder).value_or(std::filesystem::path());
    }
}
