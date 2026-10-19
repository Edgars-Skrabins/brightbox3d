#pragma once

#include <array>

namespace BrightBox3D::Editor::EditorLayout
{
    inline constexpr const char* ContentFolder = "EditorContent";
    inline constexpr const char* SourceTreeEditorFolder = "Editor";
    inline constexpr const char* SourceTreeContentFolder = "Content";
    inline constexpr const char* FontsFolder = "Fonts";
    inline constexpr const char* ThirdPartyFolder = "ThirdParty";
    inline constexpr const char* IconFontFolder = "Lucide";
    inline constexpr const char* IconFontFileName = "lucide.ttf";
    inline constexpr const char* AssetTemplatesFolder = "AssetTemplates";
    inline constexpr const char* ScriptTemplatesFolder = "ScriptTemplates";
    inline constexpr const char* TemplateFileSuffix = ".in";

    inline constexpr const char* UserSettingsFolder = ".brightbox3d";
    inline constexpr const char* PortableSettingsFolder = "UserSettings";
    inline constexpr const char* LayoutFileName = "EditorLayout.ini";
    inline constexpr const char* PreferencesFileName = "Preferences.json";
    inline constexpr const char* PanelsFileName = "EditorPanels.json";
    inline constexpr const char* RecentProjectsFileName = "RecentProjects.json";

    inline constexpr std::array<const char*, 5> FallbackUIFonts = {
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };

#if defined(_WIN32)
    inline constexpr const char* CMakeExecutable = "cmake.exe";
    inline constexpr const char* NinjaExecutable = "ninja.exe";
    inline constexpr char PathListSeparator = ';';
#else
    inline constexpr const char* CMakeExecutable = "cmake";
    inline constexpr const char* NinjaExecutable = "ninja";
    inline constexpr char PathListSeparator = ':';
#endif

    inline constexpr const char* PathVariable = "PATH";
    inline constexpr std::array<const char*, 4> CMakeInstallLocations = {
        "/opt/homebrew/bin/cmake",
        "/usr/local/bin/cmake",
        "/Applications/CMake.app/Contents/bin/cmake",
        "C:/Program Files/CMake/bin/cmake.exe"
    };
    inline constexpr const char* GameBuildConfiguration = "RelWithDebInfo";
}
