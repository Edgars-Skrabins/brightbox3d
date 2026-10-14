#pragma once

#include "Commands/KeyChord.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "IconGlyphs.h"

#include <functional>
#include <optional>
#include <string>

namespace BrightBox3D::Editor
{
    enum class CommandScope
    {
        Global,
        LevelView,
        LevelContents,
        Assets,
        Inspector,
        Terminal,
        Count
    };

    enum class CommandId
    {
        NewLevel,
        Save,
        BuildGame,
        ProjectSettings,
        OpenOtherProject,
        Exit,
        Undo,
        Redo,
        Duplicate,
        Delete,
        Preferences,
        Play,
        Pause,
        Step,
        Compile,
        CancelCompile,
        ResetLayout,
        GizmoMove,
        GizmoRotate,
        GizmoScale,
        GizmoSpace,
        Snap,
        Grid,
        FocusSelection,
        Rename,
        AssetShowInFileBrowser,
        AssetOpen,
        AssetRename,
        AssetDelete,
        AssetDuplicate,
        AssetCopyPath,
        AssetSetStartupLevel,
        AssetCreateVariant,
        AssetReimport,
        AssetRefresh,
        AssetsOneColumnLayout,
        AssetsTwoColumnLayout,
        Count
    };

    struct EditorCommand
    {
        const char* key = "";
        Icon icon = Icon::None;
        EditorText label = EditorText::Count;
        EditorText tooltip = EditorText::Count;
        CommandScope scope = CommandScope::Global;
        KeyChord defaultShortcut;
        std::function<bool()> isEnabled;
        std::function<bool()> isChecked;
        std::function<void()> execute;
        std::function<std::string()> dynamicLabel;
        std::function<Icon()> dynamicIcon;
        std::optional<EditorColor> checkedColor;
    };
}
