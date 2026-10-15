#include "Theme.h"

#include "Core/FileSystem.h"
#include "Core/Utf8.h"
#include "EditorPreferences.h"
#include "IconGlyphs.h"

#include <imgui.h>

#include <cmath>
#include <filesystem>
#include <iterator>
#include <vector>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr float EditorMetrics[] = {
            4.0f,
            110.0f,
            40.0f,
            120.0f,
            260.0f,
            10.0f,
            8.0f,
            5.0f,
            8.0f,
            2.0f,
            760.0f,
            40.0f,
            60.0f,
            160.0f,
            560.0f,
            640.0f,
            520.0f,
            480.0f,
            640.0f,
            420.0f
        };

        static_assert(std::size(EditorMetrics) == static_cast<size_t>(EditorMetric::Count));

        float& ThemeScale()
        {
            static float scale = 1.0f;
            return scale;
        }

        ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
        {
            return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
        }

        ImVec4 WithAlpha(ImVec4 color, float alpha)
        {
            color.w *= alpha;
            return color;
        }

        std::filesystem::path EditorContentDirectory()
        {
            std::filesystem::path directory = Core::FileSystem::ExecutableDirectory();
            for (int i = 0; i < 6 && !directory.empty(); ++i)
            {
                if (std::filesystem::exists(directory / "EditorContent"))
                {
                    return directory / "EditorContent";
                }
                if (std::filesystem::exists(directory / "Editor" / "Content"))
                {
                    return directory / "Editor" / "Content";
                }
                directory = directory.parent_path();
            }
            return {};
        }
    }

    void PushSelectionColors()
    {
        ImGui::PushStyleColor(ImGuiCol_Header, EditorColorVec4(EditorColor::Accent, 0.28f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorColorVec4(EditorColor::RowHovered));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorColorVec4(EditorColor::Accent, 0.38f));
    }

    void PopSelectionColors()
    {
        ImGui::PopStyleColor(3);
    }

    void ApplyEditorTheme(float scale)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        float fontSize = style.FontSizeBase;
        style = ImGuiStyle();
        style.FontSizeBase = fontSize;
        style.WindowRounding = 6.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 6.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.ScrollbarRounding = 6.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.WindowPadding = ImVec2(10, 8);
        style.FramePadding = ImVec2(8, 4);
        style.ItemSpacing = ImVec2(8, 5);
        style.ItemInnerSpacing = ImVec2(6, 4);
        style.CellPadding = ImVec2(6, 3);
        style.IndentSpacing = 16.0f;
        style.ScrollbarSize = 10.0f;
        style.GrabMinSize = 10.0f;
        style.TabBarBorderSize = 1.0f;
        style.TabBarOverlineSize = 2.0f;
        style.DockingSeparatorSize = 3.0f;
        style.SeparatorTextBorderSize = 1.0f;
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesToNodes;
        style.TreeLinesSize = 1.0f;

        const EditorPreferences& preferences = EditorPreferences::Get();
        ImVec4 accent = preferences.GetColorVec4(EditorColor::Accent);
        ImVec4 background = preferences.GetColorVec4(EditorColor::Background);
        ImVec4 panel = preferences.GetColorVec4(EditorColor::Panel);
        ImVec4 field = preferences.GetColorVec4(EditorColor::Field);
        ImVec4 control = preferences.GetColorVec4(EditorColor::Control);
        ImVec4 controlHovered = preferences.GetColorVec4(EditorColor::ControlHovered);
        ImVec4 border = preferences.GetColorVec4(EditorColor::Border);
        ImVec4 text = preferences.GetColorVec4(EditorColor::Text);
        ImVec4 textDim = preferences.GetColorVec4(EditorColor::TextDim);
        ImVec4 rowStripe = preferences.GetColorVec4(EditorColor::RowStripe);
        ImVec4 accentBright = Mix(accent, ImVec4(1, 1, 1, 1), 0.25f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = text;
        colors[ImGuiCol_TextDisabled] = textDim;
        colors[ImGuiCol_WindowBg] = panel;
        colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_PopupBg] = WithAlpha(Mix(panel, control, 0.35f), 0.98f);
        colors[ImGuiCol_Border] = border;
        colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_FrameBg] = field;
        colors[ImGuiCol_FrameBgHovered] = Mix(field, control, 0.5f);
        colors[ImGuiCol_FrameBgActive] = control;
        colors[ImGuiCol_TitleBg] = background;
        colors[ImGuiCol_TitleBgActive] = background;
        colors[ImGuiCol_TitleBgCollapsed] = background;
        colors[ImGuiCol_MenuBarBg] = background;
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ScrollbarGrab] = control;
        colors[ImGuiCol_ScrollbarGrabHovered] = controlHovered;
        colors[ImGuiCol_ScrollbarGrabActive] = WithAlpha(accent, 0.8f);
        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_SliderGrab] = accent;
        colors[ImGuiCol_SliderGrabActive] = accentBright;
        colors[ImGuiCol_Button] = control;
        colors[ImGuiCol_ButtonHovered] = controlHovered;
        colors[ImGuiCol_ButtonActive] = WithAlpha(accent, 0.75f);
        colors[ImGuiCol_Header] = Mix(panel, control, 0.6f);
        colors[ImGuiCol_HeaderHovered] = controlHovered;
        colors[ImGuiCol_HeaderActive] = controlHovered;
        colors[ImGuiCol_Separator] = border;
        colors[ImGuiCol_SeparatorHovered] = WithAlpha(accent, 0.6f);
        colors[ImGuiCol_SeparatorActive] = accent;
        colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ResizeGripHovered] = WithAlpha(accent, 0.5f);
        colors[ImGuiCol_ResizeGripActive] = accent;
        colors[ImGuiCol_InputTextCursor] = text;
        colors[ImGuiCol_TabHovered] = controlHovered;
        colors[ImGuiCol_Tab] = background;
        colors[ImGuiCol_TabSelected] = panel;
        colors[ImGuiCol_TabSelectedOverline] = accent;
        colors[ImGuiCol_TabDimmed] = background;
        colors[ImGuiCol_TabDimmedSelected] = panel;
        colors[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(textDim, 0.35f);
        colors[ImGuiCol_DockingPreview] = WithAlpha(accent, 0.45f);
        colors[ImGuiCol_DockingEmptyBg] = background;
        colors[ImGuiCol_PlotLines] = accent;
        colors[ImGuiCol_PlotLinesHovered] = accentBright;
        colors[ImGuiCol_PlotHistogram] = accent;
        colors[ImGuiCol_PlotHistogramHovered] = accentBright;
        colors[ImGuiCol_TableHeaderBg] = control;
        colors[ImGuiCol_TableBorderStrong] = border;
        colors[ImGuiCol_TableBorderLight] = border;
        colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_TableRowBgAlt] = rowStripe;
        colors[ImGuiCol_TextLink] = accent;
        colors[ImGuiCol_TextSelectedBg] = WithAlpha(accent, 0.35f);
        colors[ImGuiCol_TreeLines] = preferences.GetColorVec4(EditorColor::TreeLines);
        colors[ImGuiCol_DragDropTarget] = accent;
        colors[ImGuiCol_UnsavedMarker] = text;
        colors[ImGuiCol_NavCursor] = accent;
        colors[ImGuiCol_NavWindowingHighlight] = WithAlpha(accent, 0.7f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.5f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.5f);
        style.ScaleAllSizes(scale);
        ThemeScale() = scale;
    }

    float ScaledMetric(EditorMetric metric)
    {
        return EditorMetrics[static_cast<size_t>(metric)] * ThemeScale();
    }

    void DrawCenteredIcon(Icon icon, const ImVec2& min, const ImVec2& max)
    {
        DrawCenteredIcon(icon, min, max, ImGui::GetFontSize(), ImGui::GetColorU32(ImGuiCol_Text));
    }

    void DrawCenteredIcon(Icon icon, const ImVec2& min, const ImVec2& max, float size, unsigned int color)
    {
        const char* glyphText = IconGlyph(icon);
        std::vector<uint32_t> codepoints = Core::Utf8::Decode(glyphText);
        if (codepoints.empty())
        {
            return;
        }
        const ImFontGlyph* glyph = ImGui::GetFont()->GetFontBaked(size)->FindGlyph(static_cast<ImWchar>(codepoints[0]));
        ImVec2 position(min.x + (max.x - min.x - (glyph->X1 - glyph->X0)) * 0.5f - glyph->X0, min.y + (max.y - min.y - (glyph->Y1 - glyph->Y0)) * 0.5f - glyph->Y0);
        ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), size, ImVec2(std::floor(position.x), std::floor(position.y)), color, glyphText);
    }

    bool IconButton(Icon icon, EditorText tooltip)
    {
        return IconButton(icon, std::string(Text(tooltip)));
    }

    bool IconButton(Icon icon, const std::string& tooltip)
    {
        float size = ImGui::GetFrameHeight();
        ImGui::PushID(static_cast<int>(icon));
        bool pressed = ImGui::Button("##IconButton", ImVec2(size, size));
        ImGui::PopID();
        DrawCenteredIcon(icon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("%s", tooltip.c_str());
        }
        return pressed;
    }

    void LoadEditorFonts(float contentScale)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        const char* candidates[] = {
            "/System/Library/Fonts/SFNS.ttf",
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "C:/Windows/Fonts/segoeui.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
        };
        static const ImWchar iconRanges[] = { IconRangeMin, IconRangeMax, 0 };
        bool loaded = false;
        for (const char* candidate : candidates)
        {
            if (std::filesystem::exists(candidate))
            {
                ImFontConfig config;
                config.GlyphExcludeRanges = iconRanges;
                if (io.Fonts->AddFontFromFileTTF(candidate, 14.0f, &config))
                {
                    ImGui::GetStyle().FontSizeBase = 14.0f;
                    loaded = true;
                    break;
                }
            }
        }
        if (!loaded)
        {
            io.Fonts->AddFontDefault();
        }
        std::filesystem::path iconFont = EditorContentDirectory() / "Fonts" / "ThirdParty" / "Lucide" / "lucide.ttf";
        if (std::filesystem::exists(iconFont))
        {
            ImFontConfig config;
            config.MergeMode = true;
            config.GlyphMinAdvanceX = 15.0f;
            config.GlyphOffset = ImVec2(0.0f, 2.0f);
            io.Fonts->AddFontFromFileTTF(iconFont.string().c_str(), 14.0f, &config);
        }
    }
}
