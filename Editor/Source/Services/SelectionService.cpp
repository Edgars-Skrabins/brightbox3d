#include "Services/SelectionService.h"

#include "EditorContext.h"

#include <algorithm>

namespace BrightBox3D::Editor
{
    void SelectionService::SetSelection(std::vector<Core::Guid> selection)
    {
        Level::Level* level = m_context.GetLevelService().GetActiveLevel();
        m_selection.clear();
        for (const Core::Guid& id : selection)
        {
            if (level && level->FindObject(id))
            {
                m_selection.push_back(id);
            }
        }
        m_focus = m_selection.empty() ? SelectionFocus::None : SelectionFocus::Objects;
    }

    Level::LevelObject* SelectionService::GetPrimarySelection() const
    {
        Level::Level* level = m_context.GetLevelService().GetActiveLevel();
        if (!level || m_selection.empty() || m_focus != SelectionFocus::Objects)
        {
            return nullptr;
        }
        return level->FindObject(m_selection.back());
    }

    std::vector<Level::LevelObject*> SelectionService::GetSelectedObjects() const
    {
        std::vector<Level::LevelObject*> objects;
        Level::Level* level = m_context.GetLevelService().GetActiveLevel();
        if (!level)
        {
            return objects;
        }
        for (const Core::Guid& id : m_selection)
        {
            if (Level::LevelObject* object = level->FindObject(id))
            {
                objects.push_back(object);
            }
        }
        return objects;
    }

    void SelectionService::Select(const Core::Guid& id, bool additive)
    {
        if (!additive)
        {
            m_selection.clear();
        }
        auto it = std::find(m_selection.begin(), m_selection.end(), id);
        if (it != m_selection.end())
        {
            if (additive)
            {
                m_selection.erase(it);
            }
        }
        else
        {
            m_selection.push_back(id);
        }
        m_focus = m_selection.empty() ? SelectionFocus::None : SelectionFocus::Objects;
    }

    void SelectionService::ClearSelection()
    {
        m_selection.clear();
        if (m_focus == SelectionFocus::Objects)
        {
            m_focus = SelectionFocus::None;
        }
    }

    bool SelectionService::IsSelected(const Core::Guid& id) const
    {
        return std::find(m_selection.begin(), m_selection.end(), id) != m_selection.end();
    }

    void SelectionService::SelectAsset(const Core::Guid& asset)
    {
        m_selectedAsset = asset;
        m_focus = asset.IsValid() ? SelectionFocus::Asset : SelectionFocus::None;
    }

    void SelectionService::SelectCollection(Level::LevelCollection* collection)
    {
        m_selectedCollection = collection;
        m_selection.clear();
        m_focus = collection ? SelectionFocus::Collection : SelectionFocus::None;
    }

    void SelectionService::SelectLevel()
    {
        m_selection.clear();
        m_focus = SelectionFocus::Level;
    }

    void SelectionService::Reset()
    {
        m_selection.clear();
        m_selectedCollection = nullptr;
        m_focus = SelectionFocus::None;
    }
}
