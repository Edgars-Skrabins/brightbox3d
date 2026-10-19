#pragma once

#include "EditorText.h"
#include "IconGlyphs.h"

#include <string>

struct ImVec2;

namespace BrightBox3D::Editor
{
    enum class EditorMetric
    {
        PropertyIndent,
        PropertyLabelMinWidth,
        AssetFieldMinWidth,
        DialogButtonWidth,
        AddComponentButtonWidth,
        ToolbarExtraHeight,
        ToolbarPaddingX,
        ToolbarPaddingY,
        StatusBarPaddingX,
        StatusBarPaddingY,
        LauncherMaxWidth,
        LauncherMargin,
        LauncherTopOffset,
        LauncherButtonWidth,
        PreferencesWindowWidth,
        PreferencesWindowHeight,
        ProjectSettingsWindowWidth,
        ProjectSettingsWindowHeight,
        FloatingPanelWidth,
        FloatingPanelHeight,
        CreateScriptDialogWidth,
        Count
    };

    void ApplyEditorTheme(float scale);
    void LoadEditorFonts(float contentScale);
    void PushSelectionColors();
    void PopSelectionColors();
    float ScaledMetric(EditorMetric metric);
    void DrawCenteredIcon(Icon icon, const ImVec2& min, const ImVec2& max);
    void DrawCenteredIcon(Icon icon, const ImVec2& min, const ImVec2& max, float size, unsigned int color);
    bool IconButton(Icon icon, EditorText tooltip);
    bool IconButton(Icon icon, const std::string& tooltip);
}
