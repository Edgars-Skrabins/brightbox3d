#include "Services/ProjectService.h"

#include "Assets/AssetDatabase.h"
#include "EditorContext.h"
#include "Level/LevelSerializer.h"
#include "Runtime/GameplayMigration.h"
#include "Runtime/LevelObjectAssetMigration.h"

namespace BrightBox3D::Editor
{
    bool ProjectService::OpenProject(const std::filesystem::path& directory, std::string& error)
    {
        CloseProject();
        if (Core::Result<void> opened = m_project.Open(directory); !opened)
        {
            error = opened.GetError().GetText();
            return false;
        }
        Runtime::Project::SetCurrent(&m_project);
        Runtime::MigrateLegacyLevelObjectAssets(m_project);
        Assets::AssetDatabase::Get().Open(m_project.GetRoot());
        Runtime::MigrateLegacyGameplaySettings(m_project);
        m_context.GetLevelService().OpenStartupLevel();
        return true;
    }

    void ProjectService::CloseProject()
    {
        m_context.GetLevelObjectEditService().Reset();
        m_context.GetLevelService().Reset();
        m_context.GetSelectionService().Reset();
        m_context.GetUndoService().Clear();
        Level::LevelObjectLibrary::Get().Clear();
        Assets::AssetDatabase::Get().Close();
        Runtime::Project::SetCurrent(nullptr);
        m_project = Runtime::Project();
    }
}
