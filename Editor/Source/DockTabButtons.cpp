#include "DockTabButtons.h"

#include "EditorApplication.h"
#include "EditorWindows.h"
#include "Labels.h"
#include "Theme.h"

#include <imgui_internal.h>

#include <algorithm>
#include <string>

namespace BrightBox3D::Editor::DockTabButtons
{
    namespace
    {
        constexpr const char* AddPanelPopupId = "AddPanel";
        constexpr EditorWindow AddableWindows[] = { EditorWindow::LevelView, EditorWindow::LevelContents, EditorWindow::Inspector, EditorWindow::Assets, EditorWindow::Terminal };
        constexpr ImGuiWindowFlags OverlayFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        bool HasVisibleTabs(const ImGuiDockNode& node)
        {
            return node.IsLeafNode() && node.TabBar && node.TabBar->Tabs.Size > 0 && !node.IsHiddenTabBar() && !node.IsNoTabBar() && node.IsVisible;
        }

        float TabsEnd(const ImGuiTabBar& tabBar)
        {
            float end = 0.0f;
            for (const ImGuiTabItem& tab : tabBar.Tabs)
            {
                end = std::max(end, tab.Offset + tab.Width);
            }
            return tabBar.BarRect.Min.x + end;
        }

        void DrawPopup(EditorApplication& application, ImGuiID node)
        {
            if (!ImGui::BeginPopup(AddPanelPopupId))
            {
                return;
            }
            for (EditorWindow window : AddableWindows)
            {
                const EditorWindowInfo& info = GetWindowInfo(window);
                if (ImGui::MenuItem(IconLabel(info.icon, info.title).c_str()))
                {
                    application.AddPanelToDock(window, node);
                }
            }
            ImGui::EndPopup();
        }

        void DrawButton(EditorApplication& application, const ImGuiDockNode& node)
        {
            const ImGuiTabBar& tabBar = *node.TabBar;
            float size = ImGui::GetFrameHeight();
            float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
            float x = std::min(TabsEnd(tabBar) + spacing, tabBar.BarRect.Max.x - size);
            float y = tabBar.BarRect.Min.y + (tabBar.BarRect.GetHeight() - size) * 0.5f;
            ImGui::SetNextWindowPos(ImVec2(x, y));
            ImGui::SetNextWindowSize(ImVec2(size, size));
            ImGui::SetNextWindowViewport(node.HostWindow ? node.HostWindow->ViewportId : ImGui::GetMainViewport()->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
            std::string name = "##AddTab" + std::to_string(node.ID);
            if (ImGui::Begin(name.c_str(), nullptr, OverlayFlags))
            {
                if (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
                {
                    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
                }
                if (IconButton(Icon::Plus, EditorText::AddTab))
                {
                    ImGui::OpenPopup(AddPanelPopupId);
                }
                DrawPopup(application, node.ID);
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
        }
    }

    void Draw(EditorApplication& application)
    {
        ImGuiDockContext& docking = ImGui::GetCurrentContext()->DockContext;
        for (int index = 0; index < docking.Nodes.Data.Size; ++index)
        {
            const ImGuiDockNode* node = static_cast<const ImGuiDockNode*>(docking.Nodes.Data[index].val_p);
            if (node && HasVisibleTabs(*node))
            {
                DrawButton(application, *node);
            }
        }
    }
}
