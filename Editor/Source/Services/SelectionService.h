#pragma once

#include "Core/Guid.h"
#include "Level/Level.h"

#include <functional>
#include <vector>

namespace BrightBox3D::Editor
{
    class EditorContext;

    enum class SelectionFocus
    {
        None,
        Objects,
        Asset,
        Collection,
        Level
    };

    enum class SelectionMode
    {
        Replace,
        Toggle
    };

    class SelectionService
    {
    public:
        explicit SelectionService(EditorContext& context) : m_context(context) {}

        const std::vector<Core::Guid>& GetSelection() const { return m_selection; }
        void SetSelection(std::vector<Core::Guid> selection);
        Level::LevelObject* GetPrimarySelection() const;
        std::vector<Level::LevelObject*> GetSelectedObjects() const;
        void Select(const Core::Guid& id, SelectionMode mode = SelectionMode::Replace);
        void ClearSelection();
        bool IsSelected(const Core::Guid& id) const;
        void SelectAsset(const Core::Guid& asset);
        const Core::Guid& GetSelectedAsset() const { return m_selectedAsset; }
        void SelectCollection(Level::LevelCollection* collection);
        Level::LevelCollection* GetSelectedCollection() const { return m_selectedCollection; }
        void SelectLevel();
        SelectionFocus GetFocus() const { return m_focus; }
        void Reset();

        std::function<void(const Core::Guid&)> OnFocusObjectRequested;

    private:
        EditorContext& m_context;
        std::vector<Core::Guid> m_selection;
        Core::Guid m_selectedAsset;
        Level::LevelCollection* m_selectedCollection = nullptr;
        SelectionFocus m_focus = SelectionFocus::None;
    };
}
