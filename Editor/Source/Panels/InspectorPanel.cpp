#include "Panels/InspectorPanel.h"

#include "Assets/AssetDatabase.h"
#include "EditorApplication.h"
#include "EditorText.h"
#include "EditorWindows.h"
#include "Panels/Inspector/LevelInspector.h"
#include "Theme.h"

#include <imgui.h>

#include <algorithm>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr const char* LockedKey = "locked";
    }

    InspectorPanel::InspectorPanel(EditorApplication& application, int instanceIndex) : EditorPanel(EditorWindow::Inspector, instanceIndex), m_application(application), m_object(application), m_asset(application)
    {
    }

    InspectorPanel::~InspectorPanel() = default;

    void InspectorPanel::SetLocked(bool locked)
    {
        m_locked = locked ? std::optional<InspectorTarget>(SelectedTarget()) : std::nullopt;
    }

    void InspectorPanel::SaveState(Serialization::JsonValue& state, Serialization::JsonAllocator& allocator) const
    {
        Serialization::SetMember(state, LockedKey, Serialization::JsonValue(m_locked.has_value() || m_lockPending), allocator);
    }

    void InspectorPanel::LoadState(const Serialization::JsonValue& state)
    {
        m_lockPending = Serialization::GetBool(state, LockedKey);
    }

    InspectorTarget InspectorPanel::SelectedTarget() const
    {
        SelectionService& selection = m_application.GetContext().GetSelectionService();
        InspectorTarget target;
        target.focus = selection.GetFocus();
        if (Level::LevelObject* object = selection.GetPrimarySelection())
        {
            target.object = object->GetId();
        }
        target.asset = selection.GetSelectedAsset();
        target.collection = selection.GetSelectedCollection();
        return target;
    }

    bool InspectorPanel::TargetExists(const InspectorTarget& target) const
    {
        EditorContext& context = m_application.GetContext();
        switch (target.focus)
        {
        case SelectionFocus::Objects:
        {
            Level::Level* level = context.GetLevelService().GetActiveLevel();
            return level && level->FindObject(target.object);
        }
        case SelectionFocus::Asset:
            return Assets::AssetDatabase::Get().FindRecord(target.asset) != nullptr;
        case SelectionFocus::Collection:
            return context.GetLevelService().GetCollectionIndex(target.collection).has_value();
        case SelectionFocus::Level:
            return context.GetLevelService().GetEditLevel() != nullptr;
        default:
            return true;
        }
    }

    void InspectorPanel::DrawLockButton()
    {
        float x = ImGui::GetCursorScreenPos().x + std::max(0.0f, ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight());
        ImGui::SetCursorScreenPos(ImVec2(x, ImGui::GetCursorScreenPos().y));
        if (IconButton(m_locked ? Icon::Lock : Icon::LockOpen, EditorText::LockInspector))
        {
            SetLocked(!m_locked);
        }
    }

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

    void InspectorPanel::DrawMissingTarget()
    {
        ImGui::TextDisabled("%s", Text(EditorText::LockedTargetMissing));
        if (ImGui::Button(Text(EditorText::Unlock)))
        {
            m_locked.reset();
        }
    }

    void InspectorPanel::DrawTarget(const InspectorTarget& target)
    {
        EditorContext& context = m_application.GetContext();
        switch (target.focus)
        {
        case SelectionFocus::Objects:
            if (m_locked)
            {
                m_object.Draw(*context.GetLevelService().GetActiveLevel()->FindObject(target.object));
            }
            else
            {
                DrawSelectedObjects();
            }
            break;
        case SelectionFocus::Asset:
            m_asset.Draw(target.asset);
            break;
        case SelectionFocus::Collection:
            LevelInspector::DrawCollection(context, target.collection);
            break;
        case SelectionFocus::Level:
            LevelInspector::DrawLevel(context);
            break;
        default:
            ImGui::TextDisabled("%s", Text(EditorText::SelectObjectOrAsset));
            break;
        }
    }

    void InspectorPanel::Draw()
    {
        if (!BeginWindow())
        {
            ImGui::End();
            return;
        }
        if (m_lockPending)
        {
            m_lockPending = false;
            SetLocked(true);
        }
        DrawLockButton();
        if (m_locked && !TargetExists(*m_locked))
        {
            DrawMissingTarget();
        }
        else
        {
            DrawTarget(m_locked ? *m_locked : SelectedTarget());
        }
        ImGui::End();
    }
}
