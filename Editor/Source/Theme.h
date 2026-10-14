#pragma once

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
        Count
    };

    void ApplyEditorTheme(float scale);
    void LoadEditorFonts(float contentScale);
    void PushSelectionColors();
    void PopSelectionColors();
    float ScaledMetric(EditorMetric metric);
}
