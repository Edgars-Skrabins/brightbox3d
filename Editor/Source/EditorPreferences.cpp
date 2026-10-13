#include "EditorPreferences.h"

#include "Core/Log.h"
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

        const EditorColorInfo ColorInfos[] = {
            { "Accent", "Accent", "Interface", Hex(0xFF8A3D) },
            { "Background", "Background", "Interface", Hex(0x151517) },
            { "Panel", "Panel", "Interface", Hex(0x1E1E21) },
            { "Field", "Input Field", "Interface", Hex(0x141416) },
            { "Control", "Button", "Interface", Hex(0x2C2C30) },
            { "ControlHovered", "Button Hovered", "Interface", Hex(0x38383D) },
            { "Border", "Border", "Interface", Hex(0x0E0E10) },
            { "Text", "Text", "Interface", Hex(0xE4E4E7) },
            { "TextDim", "Dim Text", "Interface", Hex(0x8B8B93) },
            { "RowStripe", "Row Stripe", "Interface", Hex(0xFFFFFF, 0.025f) },
            { "RowHovered", "Row Hovered", "Interface", Hex(0xFFFFFF, 0.05f) },
            { "TreeLines", "Hierarchy Lines", "Interface", Hex(0x8B8B93, 0.25f) },
            { "Linked", "Linked LevelObject", "Status", Hex(0x5AA9FF) },
            { "Missing", "Missing Type", "Status", Hex(0xFF5C5C) },
            { "Override", "Property Override", "Status", Hex(0x5AA9FF) },
            { "PlayMode", "PlayMode", "Status", Hex(0x3D8BFF) },
            { "Success", "Success", "Status", Hex(0x7BD88F) },
            { "Warning", "Warning", "Status", Hex(0xFFC452) },
            { "Error", "Error", "Status", Hex(0xFF6B6B) },
            { "GridMinor", "Grid", "Level View", Hex(0xCCCCCC, 0.28f) },
            { "GridMajor", "Grid Major Lines", "Level View", Hex(0xE6E6E6, 0.55f) },
            { "AxisX", "X Axis", "Level View", Hex(0xF24D4D, 0.8f) },
            { "AxisZ", "Z Axis", "Level View", Hex(0x4D4DF2, 0.8f) },
            { "LightGizmo", "Light", "Level View", Hex(0xFFE666, 0.9f) },
            { "CameraGizmo", "Camera", "Level View", Hex(0xC8D2FF) },
            { "PlayerStartGizmo", "PlayerStart", "Level View", Hex(0x78F08C) },
            { "Collider", "Collider", "Level View", Hex(0x59FF66, 0.9f) },
            { "Trigger", "Trigger", "Level View", Hex(0x4DCCFF, 0.9f) },
            { "GizmoBackground", "Icon Background", "Level View", Hex(0x28282A, 0.8f) },
            { "AssetFolder", "Folder", "Asset Types", Hex(0xDCBE5A) },
            { "AssetLevel", "Level", "Asset Types", Hex(0x78C878) },
            { "AssetCollection", "LevelCollection", "Asset Types", Hex(0x64AA78) },
            { "AssetLevelObject", "LevelObject", "Asset Types", Hex(0x64AAFF) },
            { "AssetTemplate", "LevelObject Template", "Asset Types", Hex(0xA08CFF) },
            { "AssetMaterial", "Material", "Asset Types", Hex(0xE678A0) },
            { "AssetModel", "Model", "Asset Types", Hex(0x969696) },
            { "AssetTexture", "Texture", "Asset Types", Hex(0x78DCDC) },
            { "AssetUI", "Screen / Widget", "Asset Types", Hex(0xF0A050) },
            { "AssetStyleSheet", "StyleSheet", "Asset Types", Hex(0xC878F0) },
            { "AssetOther", "Other", "Asset Types", Hex(0xB4B4B4) }
        };

        static_assert(std::size(ColorInfos) == static_cast<size_t>(EditorColor::Count));

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
        for (size_t i = 0; i < m_colors.size(); ++i)
        {
            m_colors[i] = ColorInfos[i].defaultValue;
        }
    }

    void EditorPreferences::Load(const std::filesystem::path& path)
    {
        m_path = path;
        Serialization::JsonDocument document;
        if (!std::filesystem::exists(path))
        {
            return;
        }
        std::string error;
        if (!Serialization::LoadJsonFile(path, document, &error) || !document.IsObject())
        {
            Core::Log::Warning("Could not read editor preferences: " + error);
            return;
        }
        if (const Serialization::JsonValue* colors = Serialization::FindMember(document, "colors"); colors && colors->IsObject())
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
        ++m_revision;
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
        Serialization::SetMember(document, "colors", std::move(colors), document.GetAllocator());
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
