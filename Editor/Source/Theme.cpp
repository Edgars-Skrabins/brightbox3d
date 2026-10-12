#include "Theme.h"

#include <imgui.h>

#include <filesystem>

namespace BrightBox3D::Editor
{
    void ApplyEditorTheme(float scale)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style = ImGuiStyle();
        style.WindowRounding = 2.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 3.0f;
        style.PopupRounding = 3.0f;
        style.ScrollbarRounding = 4.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.WindowPadding = ImVec2(8, 6);
        style.FramePadding = ImVec2(6, 3);
        style.ItemSpacing = ImVec2(6, 4);
        style.IndentSpacing = 14.0f;
        style.ScrollbarSize = 12.0f;
        style.TabBarBorderSize = 1.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.87f, 0.87f, 0.87f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 0.98f);
        colors[ImGuiCol_Border] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.36f, 0.62f, 0.95f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.36f, 0.62f, 0.95f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.46f, 0.70f, 1.00f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.36f, 0.58f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.17f, 0.36f, 0.53f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.17f, 0.36f, 0.53f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_TabSelected] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.36f, 0.62f, 0.95f, 1.00f);
        colors[ImGuiCol_TabDimmed] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.36f, 0.62f, 0.95f, 0.50f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.45f, 0.70f, 0.60f);
        colors[ImGuiCol_NavCursor] = ImVec4(0.36f, 0.62f, 0.95f, 1.00f);
        style.ScaleAllSizes(scale);
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
        for (const char* candidate : candidates)
        {
            if (std::filesystem::exists(candidate))
            {
                ImFontConfig config;
                if (io.Fonts->AddFontFromFileTTF(candidate, 14.0f, &config))
                {
                    ImGui::GetStyle().FontSizeBase = 14.0f;
                    return;
                }
            }
        }
        io.Fonts->AddFontDefault();
    }
}
