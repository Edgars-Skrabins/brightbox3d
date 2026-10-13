#include "EditorWindows.h"

#include "Core/FileSystem.h"
#include "Core/StringUtils.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <format>
#include <iterator>
#include <optional>

namespace BrightBox3D::Editor
{
    namespace
    {
        const EditorWindowInfo WindowInfos[] = {
            { "LevelContents", EditorText::LevelContentsTitle, "Level Contents" },
            { "Inspector", EditorText::InspectorTitle, "Inspector" },
            { "Assets", EditorText::AssetsTitle, "Assets" },
            { "Terminal", EditorText::TerminalTitle, "Terminal" },
            { "LevelView", EditorText::LevelViewTitle, "Level View" },
            { "ProjectSettings", EditorText::ProjectSettingsTitle, "Project Settings" },
            { "Preferences", EditorText::PreferencesTitle, "Preferences" },
        };

        static_assert(std::size(WindowInfos) == static_cast<size_t>(EditorWindow::Count));

        std::string WindowHash(const std::string& name)
        {
            return std::format("0x{:08X}", ImHashStr(name.c_str()));
        }

        std::string Identity(const EditorWindowInfo& info)
        {
            return std::string("###") + info.id;
        }
    }

    const EditorWindowInfo& GetWindowInfo(EditorWindow window)
    {
        return WindowInfos[static_cast<size_t>(window)];
    }

    std::string WindowTitle(EditorWindow window)
    {
        return std::string(Text(GetWindowInfo(window).title)) + WindowIdentity(window);
    }

    std::string WindowIdentity(EditorWindow window)
    {
        return Identity(GetWindowInfo(window));
    }

    void MigrateEditorLayout(const std::filesystem::path& path)
    {
        std::optional<std::string> text = Core::FileSystem::ReadText(path);
        if (!text)
        {
            return;
        }
        std::string migrated = *text;
        for (const EditorWindowInfo& info : WindowInfos)
        {
            std::string identity = Identity(info);
            migrated = Core::ReplaceAll(migrated, std::string("[Window][") + info.legacyTitle + "]\n", std::string("[Window][") + info.legacyTitle + identity + "]\n");
            migrated = Core::ReplaceAll(migrated, WindowHash(info.legacyTitle), WindowHash(identity));
        }
        if (migrated != *text)
        {
            Core::FileSystem::WriteText(path, migrated);
        }
    }
}
