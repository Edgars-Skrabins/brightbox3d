#pragma once

#include "EditorText.h"
#include "IconGlyphs.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

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
        Icon icon;
    };

    const EditorWindowInfo& GetWindowInfo(EditorWindow window);
    std::string WindowTitle(EditorWindow window);
    std::string WindowIdentity(EditorWindow window);
    std::optional<EditorWindow> FindWindowById(std::string_view id);
    void MigrateEditorLayout(const std::filesystem::path& path);
}
