#pragma once

#include "Math/Color.h"

#include <imgui.h>

#include <array>
#include <cstdint>
#include <filesystem>

namespace BrightBox3D::Editor
{
    enum class EditorColor
    {
        Accent,
        Background,
        Panel,
        Field,
        Control,
        ControlHovered,
        Border,
        Text,
        TextDim,
        RowStripe,
        RowHovered,
        TreeLines,
        Linked,
        Missing,
        Override,
        PlayMode,
        Success,
        Warning,
        Error,
        GridMinor,
        GridMajor,
        AxisX,
        AxisZ,
        LightGizmo,
        CameraGizmo,
        PlayerStartGizmo,
        Collider,
        Trigger,
        GizmoBackground,
        AssetFolder,
        AssetLevel,
        AssetCollection,
        AssetLevelObject,
        AssetTemplate,
        AssetMaterial,
        AssetModel,
        AssetTexture,
        AssetUI,
        AssetStyleSheet,
        AssetOther,
        Count
    };

    struct EditorColorInfo
    {
        const char* key;
        const char* label;
        const char* category;
        Math::Color defaultValue;
    };

    class EditorPreferences
    {
    public:
        static EditorPreferences& Get();
        static const EditorColorInfo& GetColorInfo(EditorColor color);

        void Load(const std::filesystem::path& path);
        void Save() const;

        const Math::Color& GetColor(EditorColor color) const { return m_colors[static_cast<size_t>(color)]; }
        ImVec4 GetColorVec4(EditorColor color, float alpha = 1.0f) const;
        ImU32 GetColorU32(EditorColor color, float alpha = 1.0f) const;
        void SetColor(EditorColor color, const Math::Color& value);
        void ResetColor(EditorColor color);
        void ResetAllColors();
        bool IsDefault(EditorColor color) const;
        bool HasChanges() const;

        uint32_t GetRevision() const { return m_revision; }

    private:
        EditorPreferences();

        std::array<Math::Color, static_cast<size_t>(EditorColor::Count)> m_colors;
        std::filesystem::path m_path;
        uint32_t m_revision = 1;
    };

    inline ImVec4 EditorColorVec4(EditorColor color, float alpha = 1.0f)
    {
        return EditorPreferences::Get().GetColorVec4(color, alpha);
    }

    inline ImU32 EditorColorU32(EditorColor color, float alpha = 1.0f)
    {
        return EditorPreferences::Get().GetColorU32(color, alpha);
    }
}
