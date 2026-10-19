#pragma once

#include "Services/AssetCreationService.h"
#include "Services/LevelObjectEditService.h"
#include "Services/LevelService.h"
#include "Services/ObjectEditService.h"
#include "Services/ProjectService.h"
#include "Services/ScriptLibrary.h"
#include "Services/SelectionService.h"
#include "Services/UndoService.h"

namespace BrightBox3D::Editor
{
    class EditorContext
    {
    public:
        EditorContext();
        ~EditorContext();

        EditorContext(const EditorContext&) = delete;
        EditorContext& operator=(const EditorContext&) = delete;

        ProjectService& GetProjectService() { return m_project; }
        const ProjectService& GetProjectService() const { return m_project; }
        LevelService& GetLevelService() { return m_levels; }
        const LevelService& GetLevelService() const { return m_levels; }
        SelectionService& GetSelectionService() { return m_selection; }
        const SelectionService& GetSelectionService() const { return m_selection; }
        UndoService& GetUndoService() { return m_undo; }
        LevelObjectEditService& GetLevelObjectEditService() { return m_levelObjectEditing; }
        const LevelObjectEditService& GetLevelObjectEditService() const { return m_levelObjectEditing; }
        AssetCreationService& GetAssetCreationService() { return m_assetCreation; }
        ObjectEditService& GetObjectEditService() { return m_objectEditing; }
        ScriptLibrary& GetScriptLibrary() { return m_scripts; }

    private:
        ProjectService m_project;
        LevelService m_levels;
        SelectionService m_selection;
        UndoService m_undo;
        LevelObjectEditService m_levelObjectEditing;
        AssetCreationService m_assetCreation;
        ObjectEditService m_objectEditing;
        ScriptLibrary m_scripts;
    };
}
