#include "EditorPreferences.h"

#include "Commands/CommandRegistry.h"
#include "Commands/ShortcutBindings.h"
#include "Core/Log.h"
#include "Framework/PlayerStart.h"
#include "Labels.h"
#include "Level/LevelObject.h"
#include "Serialization/Json.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr Math::Color Hex(uint32_t rgb, float alpha = 1.0f)
        {
            return { ((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha };
        }

        constexpr const char* ColorsSection = "colors";
        constexpr const char* ShortcutsSection = "shortcuts";
        constexpr const char* AssetsSection = "assets";
        constexpr const char* AssetsLayoutKey = "layout";
        constexpr const char* AssetIconSizeKey = "iconSize";
        constexpr const char* AssetsLayoutNames[] = { "OneColumn", "TwoColumn" };

        static_assert(std::size(AssetsLayoutNames) == static_cast<size_t>(AssetsLayout::Count));

        constexpr const char* TerminalSection = "terminal";
        constexpr const char* TerminalFilterKeys[] = { "showInfo", "showWarnings", "showErrors", "showEngine" };

        static_assert(std::size(TerminalFilterKeys) == static_cast<size_t>(TerminalFilter::Count));

        bool IsValidAssetIconSize(int size)
        {
            return size == EditorPreferences::AssetListIconSize || (size >= EditorPreferences::MinAssetIconSize && size <= EditorPreferences::MaxAssetIconSize);
        }

        const EditorColorInfo ColorInfos[] = {
            { "Accent", EditorText::ColorAccent, EditorColorCategory::Interface, Hex(0xFF8A3D) },
            { "Background", EditorText::ColorBackground, EditorColorCategory::Interface, Hex(0x151517) },
            { "Panel", EditorText::ColorPanel, EditorColorCategory::Interface, Hex(0x1E1E21) },
            { "Field", EditorText::ColorField, EditorColorCategory::Interface, Hex(0x141416) },
            { "Control", EditorText::ColorControl, EditorColorCategory::Interface, Hex(0x2C2C30) },
            { "ControlHovered", EditorText::ColorControlHovered, EditorColorCategory::Interface, Hex(0x38383D) },
            { "Border", EditorText::ColorBorder, EditorColorCategory::Interface, Hex(0x0E0E10) },
            { "Text", EditorText::ColorText, EditorColorCategory::Interface, Hex(0xE4E4E7) },
            { "TextDim", EditorText::ColorTextDim, EditorColorCategory::Interface, Hex(0x8B8B93) },
            { "RowStripe", EditorText::ColorRowStripe, EditorColorCategory::Interface, Hex(0xFFFFFF, 0.025f) },
            { "RowHovered", EditorText::ColorRowHovered, EditorColorCategory::Interface, Hex(0xFFFFFF, 0.05f) },
            { "TreeLines", EditorText::ColorTreeLines, EditorColorCategory::Interface, Hex(0x8B8B93, 0.25f) },
            { "Linked", EditorText::ColorLinked, EditorColorCategory::Status, Hex(0x5AA9FF), { &TypeName<Level::LevelObject> } },
            { "Missing", EditorText::ColorMissing, EditorColorCategory::Status, Hex(0xFF5C5C) },
            { "Override", EditorText::ColorOverride, EditorColorCategory::Status, Hex(0x5AA9FF) },
            { "PlayMode", EditorText::ColorPlayMode, EditorColorCategory::Status, Hex(0x3D8BFF) },
            { "Success", EditorText::ColorSuccess, EditorColorCategory::Status, Hex(0x7BD88F) },
            { "Warning", EditorText::ColorWarning, EditorColorCategory::Status, Hex(0xFFC452) },
            { "Error", EditorText::ColorError, EditorColorCategory::Status, Hex(0xFF6B6B) },
            { "GridMinor", EditorText::ColorGridMinor, EditorColorCategory::LevelView, Hex(0xCCCCCC, 0.28f) },
            { "GridMajor", EditorText::ColorGridMajor, EditorColorCategory::LevelView, Hex(0xE6E6E6, 0.55f) },
            { "AxisX", EditorText::ColorAxisX, EditorColorCategory::LevelView, Hex(0xF24D4D, 0.8f) },
            { "AxisZ", EditorText::ColorAxisZ, EditorColorCategory::LevelView, Hex(0x4D4DF2, 0.8f) },
            { "LightGizmo", EditorText::ColorLightGizmo, EditorColorCategory::LevelView, Hex(0xFFE666, 0.9f) },
            { "CameraGizmo", EditorText::ColorCameraGizmo, EditorColorCategory::LevelView, Hex(0xC8D2FF) },
            { "PlayerStartGizmo", EditorText::ColorPlayerStartGizmo, EditorColorCategory::LevelView, Hex(0x78F08C), { &TypeName<Framework::PlayerStart> } },
            { "Collider", EditorText::ColorCollider, EditorColorCategory::LevelView, Hex(0x59FF66, 0.9f) },
            { "Trigger", EditorText::ColorTrigger, EditorColorCategory::LevelView, Hex(0x4DCCFF, 0.9f) },
            { "GizmoBackground", EditorText::ColorGizmoBackground, EditorColorCategory::LevelView, Hex(0x28282A, 0.8f) },
            { "AssetFolder", EditorText::ColorAssetFolder, EditorColorCategory::AssetTypes, Hex(0xDCBE5A), { &AssetTypeLabel<Assets::AssetType::Folder> } },
            { "AssetLevel", EditorText::ColorAssetLevel, EditorColorCategory::AssetTypes, Hex(0x78C878), { &AssetTypeLabel<Assets::AssetType::Level> } },
            { "AssetCollection", EditorText::ColorAssetCollection, EditorColorCategory::AssetTypes, Hex(0x64AA78), { &AssetTypeLabel<Assets::AssetType::LevelCollection> } },
            { "AssetLevelObject", EditorText::ColorAssetLevelObject, EditorColorCategory::AssetTypes, Hex(0x64AAFF), { &AssetTypeLabel<Assets::AssetType::LevelObjectVariant> } },
            { "AssetTemplate", EditorText::ColorAssetTemplate, EditorColorCategory::AssetTypes, Hex(0xA08CFF), { &AssetTypeLabel<Assets::AssetType::LevelObjectTemplate> } },
            { "AssetMaterial", EditorText::ColorAssetMaterial, EditorColorCategory::AssetTypes, Hex(0xE678A0), { &AssetTypeLabel<Assets::AssetType::Material> } },
            { "AssetModel", EditorText::ColorAssetModel, EditorColorCategory::AssetTypes, Hex(0x969696), { &AssetTypeLabel<Assets::AssetType::Model> } },
            { "AssetTexture", EditorText::ColorAssetTexture, EditorColorCategory::AssetTypes, Hex(0x78DCDC), { &AssetTypeLabel<Assets::AssetType::Texture> } },
            { "AssetUI", EditorText::ColorAssetUI, EditorColorCategory::AssetTypes, Hex(0xF0A050), { &AssetTypeLabel<Assets::AssetType::Screen>, &AssetTypeLabel<Assets::AssetType::Widget> } },
            { "AssetStyleSheet", EditorText::ColorAssetStyleSheet, EditorColorCategory::AssetTypes, Hex(0xC878F0), { &AssetTypeLabel<Assets::AssetType::StyleSheet> } },
            { "AssetScript", EditorText::ColorAssetScript, EditorColorCategory::AssetTypes, Hex(0x5FB3F0) },
            { "AssetOther", EditorText::ColorAssetOther, EditorColorCategory::AssetTypes, Hex(0xB4B4B4) }
        };

        static_assert(std::size(ColorInfos) == static_cast<size_t>(EditorColor::Count));

        const EditorText ColorCategoryLabels[] = { EditorText::ColorCategoryInterface, EditorText::ColorCategoryStatus, EditorText::ColorCategoryLevelView, EditorText::ColorCategoryAssetTypes };
        static_assert(std::size(ColorCategoryLabels) == static_cast<size_t>(EditorColorCategory::Count));

        std::string ToHex(const Math::Color& color)
        {
            auto channel = [](float value) { return static_cast<unsigned int>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f); };
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X", channel(color.r), channel(color.g), channel(color.b), channel(color.a));
            return buffer;
        }

        bool FromHex(const std::string& text, Math::Color& color)
        {
            if (text.size() != 9 || text[0] != '#')
            {
                return false;
            }
            unsigned int value = 0;
            if (std::sscanf(text.c_str() + 1, "%8x", &value) != 1)
            {
                return false;
            }
            color = Math::Color(((value >> 24) & 0xFF) / 255.0f, ((value >> 16) & 0xFF) / 255.0f, ((value >> 8) & 0xFF) / 255.0f, (value & 0xFF) / 255.0f);
            return true;
        }

        bool SameAsStored(const Math::Color& a, const Math::Color& b)
        {
            return ToHex(a) == ToHex(b);
        }
    }

    std::string EditorColorLabel(EditorColor color)
    {
        const EditorColorInfo& info = ColorInfos[static_cast<size_t>(color)];
        if (info.labelSubjects[1])
        {
            return Text(info.label, info.labelSubjects[0](), info.labelSubjects[1]());
        }
        if (info.labelSubjects[0])
        {
            return Text(info.label, info.labelSubjects[0]());
        }
        return Text(info.label);
    }

    EditorText ColorCategoryLabel(EditorColorCategory category)
    {
        return ColorCategoryLabels[static_cast<size_t>(category)];
    }

    EditorPreferences& EditorPreferences::Get()
    {
        static EditorPreferences* preferences = new EditorPreferences();
        return *preferences;
    }

    const EditorColorInfo& EditorPreferences::GetColorInfo(EditorColor color)
    {
        return ColorInfos[static_cast<size_t>(color)];
    }

    EditorPreferences::EditorPreferences()
    {
        m_terminalFilters.fill(true);
        for (size_t i = 0; i < m_colors.size(); ++i)
        {
            m_colors[i] = ColorInfos[i].defaultValue;
        }
    }

    void EditorPreferences::Load(const std::filesystem::path& path)
    {
        m_path = path;
        m_assetsLayout = DefaultAssetsLayout;
        m_assetIconSize = DefaultAssetIconSize;
        m_terminalFilters.fill(true);
        Serialization::JsonDocument document;
        if (!std::filesystem::exists(path))
        {
            return;
        }
        if (Core::Result<void> loaded = Serialization::LoadJsonFile(path, document); !loaded)
        {
            Core::Log::Warning(Text(EditorText::PreferencesLoadFailed, loaded.GetError().GetText()));
            return;
        }
        if (!document.IsObject())
        {
            Core::Log::Warning(Text(EditorText::PreferencesLoadFailed, Text(EditorText::TextFileNotObject)));
            return;
        }
        if (const Serialization::JsonValue* colors = Serialization::FindMember(document, ColorsSection); colors && colors->IsObject())
        {
            for (size_t i = 0; i < m_colors.size(); ++i)
            {
                Math::Color value;
                if (FromHex(Serialization::GetString(*colors, ColorInfos[i].key), value))
                {
                    m_colors[i] = value;
                }
            }
        }
        if (const Serialization::JsonValue* shortcuts = Serialization::FindMember(document, ShortcutsSection))
        {
            ReadShortcutBindings(CommandRegistry::Get(), *shortcuts);
        }
        if (const Serialization::JsonValue* assets = Serialization::FindMember(document, AssetsSection); assets && assets->IsObject())
        {
            LoadAssets(*assets);
        }
        if (const Serialization::JsonValue* terminal = Serialization::FindMember(document, TerminalSection); terminal && terminal->IsObject())
        {
            LoadTerminal(*terminal);
        }
        ++m_revision;
    }

    void EditorPreferences::LoadTerminal(const Serialization::JsonValue& terminal)
    {
        for (size_t i = 0; i < m_terminalFilters.size(); ++i)
        {
            const Serialization::JsonValue* value = Serialization::FindMember(terminal, TerminalFilterKeys[i]);
            if (value && value->IsBool())
            {
                m_terminalFilters[i] = value->GetBool();
            }
        }
    }

    void EditorPreferences::LoadAssets(const Serialization::JsonValue& assets)
    {
        std::string layout = Serialization::GetString(assets, AssetsLayoutKey);
        for (size_t i = 0; i < std::size(AssetsLayoutNames); ++i)
        {
            if (layout == AssetsLayoutNames[i])
            {
                m_assetsLayout = static_cast<AssetsLayout>(i);
            }
        }
        const Serialization::JsonValue* iconSize = Serialization::FindMember(assets, AssetIconSizeKey);
        if (iconSize && iconSize->IsInt() && IsValidAssetIconSize(iconSize->GetInt()))
        {
            m_assetIconSize = iconSize->GetInt();
        }
    }

    void EditorPreferences::Save() const
    {
        if (m_path.empty())
        {
            return;
        }
        Serialization::JsonDocument document;
        document.SetObject();
        Serialization::JsonValue colors(rapidjson::kObjectType);
        for (size_t i = 0; i < m_colors.size(); ++i)
        {
            if (!IsDefault(static_cast<EditorColor>(i)))
            {
                Serialization::SetMember(colors, ColorInfos[i].key, Serialization::MakeString(ToHex(m_colors[i]), document.GetAllocator()), document.GetAllocator());
            }
        }
        Serialization::SetMember(document, ColorsSection, std::move(colors), document.GetAllocator());
        Serialization::SetMember(document, ShortcutsSection, WriteShortcutBindings(CommandRegistry::Get(), document.GetAllocator()), document.GetAllocator());
        Serialization::JsonValue assets(rapidjson::kObjectType);
        Serialization::SetMember(assets, AssetsLayoutKey, Serialization::MakeString(AssetsLayoutNames[static_cast<size_t>(m_assetsLayout)], document.GetAllocator()), document.GetAllocator());
        Serialization::SetMember(assets, AssetIconSizeKey, Serialization::JsonValue(m_assetIconSize), document.GetAllocator());
        Serialization::SetMember(document, AssetsSection, std::move(assets), document.GetAllocator());
        Serialization::JsonValue terminal(rapidjson::kObjectType);
        for (size_t i = 0; i < m_terminalFilters.size(); ++i)
        {
            Serialization::SetMember(terminal, TerminalFilterKeys[i], Serialization::JsonValue(m_terminalFilters[i]), document.GetAllocator());
        }
        Serialization::SetMember(document, TerminalSection, std::move(terminal), document.GetAllocator());
        Serialization::SaveJsonFile(m_path, document);
    }

    ImVec4 EditorPreferences::GetColorVec4(EditorColor color, float alpha) const
    {
        const Math::Color& value = GetColor(color);
        return ImVec4(value.r, value.g, value.b, value.a * alpha);
    }

    ImU32 EditorPreferences::GetColorU32(EditorColor color, float alpha) const
    {
        return ImGui::ColorConvertFloat4ToU32(GetColorVec4(color, alpha));
    }

    void EditorPreferences::SetColor(EditorColor color, const Math::Color& value)
    {
        m_colors[static_cast<size_t>(color)] = value;
        ++m_revision;
    }

    void EditorPreferences::ResetColor(EditorColor color)
    {
        SetColor(color, GetColorInfo(color).defaultValue);
    }

    void EditorPreferences::ResetAllColors()
    {
        for (size_t i = 0; i < m_colors.size(); ++i)
        {
            m_colors[i] = ColorInfos[i].defaultValue;
        }
        ++m_revision;
    }

    bool EditorPreferences::IsDefault(EditorColor color) const
    {
        return SameAsStored(GetColor(color), GetColorInfo(color).defaultValue);
    }

    void EditorPreferences::SetAssetsLayout(AssetsLayout layout)
    {
        m_assetsLayout = layout;
        ++m_revision;
    }

    void EditorPreferences::SetAssetIconSize(int size)
    {
        m_assetIconSize = size <= AssetListIconSize ? AssetListIconSize : std::clamp(size, MinAssetIconSize, MaxAssetIconSize);
        ++m_revision;
    }

    void EditorPreferences::SetTerminalFilter(TerminalFilter filter, bool on)
    {
        m_terminalFilters[static_cast<size_t>(filter)] = on;
        ++m_revision;
    }

    bool EditorPreferences::HasChanges() const
    {
        for (size_t i = 0; i < m_colors.size(); ++i)
        {
            if (!IsDefault(static_cast<EditorColor>(i)))
            {
                return true;
            }
        }
        return false;
    }
}
