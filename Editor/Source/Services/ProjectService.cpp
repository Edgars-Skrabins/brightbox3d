#include "Services/ProjectService.h"

#include "Assets/AssetDatabase.h"
#include "EditorContext.h"
#include "Level/LevelSerializer.h"
#include "Runtime/GameplayMigration.h"
#include "Runtime/LevelObjectAssetMigration.h"

namespace BrightBox3D::Editor
{
    Core::Result<void> ProjectService::OpenProject(const std::filesystem::path& directory)
    {
        CloseProject();
        if (Core::Result<void> opened = m_project.Open(directory); !opened)
        {
            return opened;
        }
        Runtime::Project::SetCurrent(&m_project);
        Runtime::MigrateLegacyLevelObjectAssets(m_project);
        Assets::AssetDatabase::Get().Open(m_project.GetRoot());
        m_context.GetScriptLibrary().SetRoot(m_project.GetScriptsDirectory());
        Runtime::MigrateLegacyGameplaySettings(m_project);
        m_context.GetLevelService().OpenStartupLevel();
        return {};
    }

    void ProjectService::CloseProject()
    {
        m_context.GetLevelObjectEditService().Reset();
        m_context.GetLevelService().Reset();
        m_context.GetSelectionService().Reset();
        m_context.GetUndoService().Clear();
        Level::LevelObjectLibrary::Get().Clear();
        Assets::AssetDatabase::Get().Close();
        m_context.GetScriptLibrary().SetRoot({});
        Runtime::Project::SetCurrent(nullptr);
        m_project = Runtime::Project();
    }
}
