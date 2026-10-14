#include "Panels/InspectorPanel.h"

#include "EditorApplication.h"
#include "EditorText.h"
#include "EditorWindows.h"
#include "Panels/Inspector/LevelInspector.h"

#include <imgui.h>

namespace BrightBox3D::Editor
{
    InspectorPanel::InspectorPanel(EditorApplication& application) : m_application(application), m_object(application), m_asset(application)
    {
    }

    InspectorPanel::~InspectorPanel() = default;

    void InspectorPanel::DrawSelectedObjects()
    {
        SelectionService& selection = m_application.GetContext().GetSelectionService();
        Level::LevelObject* object = selection.GetPrimarySelection();
        if (!object)
        {
            return;
        }
        if (selection.GetSelection().size() > 1)
        {
            ImGui::TextDisabled("%s", Text(EditorText::MultipleObjectsSelected, selection.GetSelection().size()).c_str());
        }
        m_object.Draw(*object);
    }

    void InspectorPanel::Draw()
    {
        if (!ImGui::Begin(WindowTitle(EditorWindow::Inspector).c_str()))
        {
            ImGui::End();
            return;
        }
        EditorContext& context = m_application.GetContext();
        switch (context.GetSelectionService().GetFocus())
        {
        case SelectionFocus::Objects:
            DrawSelectedObjects();
            break;
        case SelectionFocus::Asset:
            m_asset.Draw(context.GetSelectionService().GetSelectedAsset());
            break;
        case SelectionFocus::Collection:
            LevelInspector::DrawCollection(context);
            break;
        case SelectionFocus::Level:
            LevelInspector::DrawLevel(context);
            break;
        default:
            ImGui::TextDisabled("%s", Text(EditorText::SelectObjectOrAsset));
            break;
        }
        ImGui::End();
    }
}
