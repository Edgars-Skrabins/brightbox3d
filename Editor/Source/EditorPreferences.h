#pragma once

#include "EditorText.h"
#include "Math/Color.h"
#include "Serialization/Json.h"

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

    enum class EditorColorCategory
    {
        Interface,
        Status,
        LevelView,
        AssetTypes,
        Count
    };

    EditorText ColorCategoryLabel(EditorColorCategory category);

    enum class AssetsLayout
    {
        OneColumn,
        TwoColumn,
        Count
    };

    enum class TerminalFilter
    {
        Info,
        Warnings,
        Errors,
        Engine,
        Count
    };

    struct EditorColorInfo
    {
        const char* key;
        EditorText label;
        EditorColorCategory category;
        Math::Color defaultValue;
        std::array<const char* (*)(), 2> labelSubjects{};
    };

    std::string EditorColorLabel(EditorColor color);

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

        AssetsLayout GetAssetsLayout() const { return m_assetsLayout; }
        void SetAssetsLayout(AssetsLayout layout);
        int GetAssetIconSize() const { return m_assetIconSize; }
        void SetAssetIconSize(int size);
        bool IsAssetListMode() const { return m_assetIconSize == AssetListIconSize; }

        bool IsTerminalFilterOn(TerminalFilter filter) const { return m_terminalFilters[static_cast<size_t>(filter)]; }
        void SetTerminalFilter(TerminalFilter filter, bool on);

        static constexpr AssetsLayout DefaultAssetsLayout = AssetsLayout::TwoColumn;
        static constexpr int AssetListIconSize = 0;
        static constexpr int MinAssetIconSize = 56;
        static constexpr int MaxAssetIconSize = 160;
        static constexpr int DefaultAssetIconSize = 84;

        uint32_t GetRevision() const { return m_revision; }

    private:
        EditorPreferences();

        void LoadAssets(const Serialization::JsonValue& assets);
        void LoadTerminal(const Serialization::JsonValue& terminal);

        std::array<Math::Color, static_cast<size_t>(EditorColor::Count)> m_colors;
        AssetsLayout m_assetsLayout = DefaultAssetsLayout;
        int m_assetIconSize = DefaultAssetIconSize;
        std::array<bool, static_cast<size_t>(TerminalFilter::Count)> m_terminalFilters;
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
