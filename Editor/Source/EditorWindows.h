#pragma once

#include "EditorText.h"

#include <filesystem>
#include <string>

namespace BrightBox3D::Editor
{
    enum class EditorWindow
    {
        LevelContents,
        Inspector,
        Assets,
        Terminal,
        LevelView,
        ProjectSettings,
        Preferences,
        Count
    };

    struct EditorWindowInfo
    {
        const char* id;
        EditorText title;
        const char* legacyTitle;
    };

    const EditorWindowInfo& GetWindowInfo(EditorWindow window);
    std::string WindowTitle(EditorWindow window);
    std::string WindowIdentity(EditorWindow window);
    void MigrateEditorLayout(const std::filesystem::path& path);
}
