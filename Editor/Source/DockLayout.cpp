#include "DockLayout.h"

#include "EditorWindows.h"

#include <imgui_internal.h>

#include <array>

namespace BrightBox3D::Editor::DockLayout
{
    namespace
    {
        enum class DockArea
        {
            Center,
            Left,
            Right,
            Bottom,
            Count
        };

        struct DockSplit
        {
            DockArea area;
            ImGuiDir direction;
            float ratio;
        };

        struct DockedWindow
        {
            EditorWindow window;
            DockArea area;
        };

        constexpr DockSplit DefaultDockSplits[] = {
            { DockArea::Left, ImGuiDir_Left, 0.19f },
            { DockArea::Right, ImGuiDir_Right, 0.27f },
            { DockArea::Bottom, ImGuiDir_Down, 0.30f }
        };

        constexpr DockedWindow DefaultDockLayout[] = {
            { EditorWindow::LevelContents, DockArea::Left },
            { EditorWindow::Inspector, DockArea::Right },
            { EditorWindow::Assets, DockArea::Bottom },
            { EditorWindow::Terminal, DockArea::Bottom },
            { EditorWindow::LevelView, DockArea::Center }
        };
    }

    void ApplyDefault(ImGuiID dockspace, const ImVec2& size)
    {
        ImGui::DockBuilderRemoveNode(dockspace);
        ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace, size);
        std::array<ImGuiID, static_cast<size_t>(DockArea::Count)> nodes{};
        ImGuiID& center = nodes[static_cast<size_t>(DockArea::Center)];
        center = dockspace;
        for (const DockSplit& split : DefaultDockSplits)
        {
            nodes[static_cast<size_t>(split.area)] = ImGui::DockBuilderSplitNode(center, split.direction, split.ratio, nullptr, &center);
        }
        for (const DockedWindow& docked : DefaultDockLayout)
        {
            ImGui::DockBuilderDockWindow(WindowIdentity(docked.window).c_str(), nodes[static_cast<size_t>(docked.area)]);
        }
        ImGui::DockBuilderFinish(dockspace);
    }
}
