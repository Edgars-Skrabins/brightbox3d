#pragma once

#include "Core/Event.h"
#include "Core/Guid.h"
#include "Level/Level.h"
#include "UndoStack.h"

#include <memory>
#include <vector>

namespace BrightBox3D::Editor
{
    class EditorContext;

    class LevelObjectEditService
    {
    public:
        explicit LevelObjectEditService(EditorContext& context);
        ~LevelObjectEditService();

        bool IsEditingLevelObject() const { return m_editedLevelObject.IsValid(); }
        const Core::Guid& GetEditedLevelObject() const { return m_editedLevelObject; }
        Level::Level* GetStashedLevel() const { return m_stashedLevel.get(); }
        bool OpenLevelObjectEditor(const Core::Guid& asset);
        void CloseLevelObjectEditor(bool save);
        bool SaveLevelObjectEditor();
        bool IsDirty() const { return m_dirty; }
        void MarkDirty() { m_dirty = true; }
        void Reset();

    private:
        void OnAssetChanged(const Core::Guid& guid);

        EditorContext& m_context;
        std::unique_ptr<Level::Level> m_stashedLevel;
        std::vector<Core::Guid> m_stashedSelection;
        UndoStack m_stashedUndo;
        Core::Guid m_editedLevelObject;
        bool m_dirty = false;
        Core::EventHandle m_assetChangedHandle = 0;
    };
}
