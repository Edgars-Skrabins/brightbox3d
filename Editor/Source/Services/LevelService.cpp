#include "Services/LevelService.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
#include "Core/Log.h"
#include "EditorContext.h"
#include "EditorText.h"
#include "Level/LevelSerializer.h"
#include "PlaySession.h"
#include "Rendering/BuiltinAssets.h"
#include "Runtime/ProjectLayout.h"

namespace BrightBox3D::Editor
{
    namespace
    {
    }

    Level::Level* LevelService::GetActiveLevel() const
    {
        if (m_playSession && m_playSession->IsActive() && m_playSession->GetLevel())
        {
            return m_playSession->GetLevel();
        }
        return m_level.get();
    }

    std::unique_ptr<Level::Level> LevelService::ExchangeEditLevel(std::unique_ptr<Level::Level> level)
    {
        std::swap(m_level, level);
        m_activeCollection = nullptr;
        ++m_levelRevision;
        return level;
    }

    bool LevelService::IsPlaying() const
    {
        return m_playSession && m_playSession->IsActive();
    }

    void LevelService::Reset()
    {
        m_level.reset();
        m_activeCollection = nullptr;
    }

    bool LevelService::OpenLevel(const Core::Guid& levelAsset)
    {
        LevelObjectEditService& editing = m_context.GetLevelObjectEditService();
        if (editing.IsEditingLevelObject())
        {
            editing.CloseLevelObjectEditor(EditorChanges::Save);
        }
        Core::Result<std::unique_ptr<Level::Level>> level = Level::LevelSerializer::LoadLevel(levelAsset);
        if (!level)
        {
            Core::Log::Error(Text(EditorText::OpenLevelFailed, level.GetError().GetText()));
            return false;
        }
        level->SetFixedDeltaTime(m_context.GetProjectService().GetProject().GetFixedTimeStep());
        m_level = std::move(*level);
        m_activeCollection = nullptr;
        m_context.GetSelectionService().Reset();
        m_context.GetUndoService().Clear();
        ++m_levelRevision;
        Core::Log::Info(Text(EditorText::OpenedLevel, m_level->GetName()));
        return true;
    }

    void LevelService::OpenStartupLevel()
    {
        Runtime::Project& project = m_context.GetProjectService().GetProject();
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        if (project.GetStartupLevel().IsValid() && database.FindRecord(project.GetStartupLevel()))
        {
            OpenLevel(project.GetStartupLevel());
            return;
        }
        auto levels = database.GetRecords(Assets::AssetType::Level);
        if (!levels.empty())
        {
            OpenLevel(levels.front().guid);
            return;
        }
        NewLevel(Runtime::ProjectLayout::LevelAssetPath(Core::Text(Core::EngineText::MainLevelName)));
        project.SetStartupLevel(m_level ? m_level->GetAsset() : Core::Guid{});
        project.Save();
    }

    bool LevelService::NewLevel(const std::string& relativePath)
    {
        Core::Result<Core::Guid> guid = Level::LevelSerializer::CreateLevelAsset(relativePath);
        if (!guid)
        {
            Core::Log::Error(Text(EditorText::CreateLevelFailed, guid.GetError().GetText()));
            return false;
        }
        if (!OpenLevel(*guid))
        {
            return false;
        }
        ObjectEditService& objects = m_context.GetObjectEditService();
        if (Level::LevelObject* sun = objects.CreateLight(Rendering::LightType::Directional))
        {
            sun->SetName(Text(EditorText::DirectionalLight));
            sun->GetTransform().SetLocalEulerAngles({ 50.0f, -30.0f, 0.0f });
            sun->GetTransform().SetLocalPosition({ 0.0f, 5.0f, 0.0f });
        }
        objects.CreateModelObject(Rendering::BuiltinAssets::Plane(), Text(EditorText::GroundName), Math::Vector3::Zero());
        m_context.GetUndoService().Clear();
        m_context.GetSelectionService().ClearSelection();
        SaveLevel();
        return true;
    }

    bool LevelService::SaveLevel()
    {
        if (IsPlaying())
        {
            Core::Log::Warning(Text(EditorText::StopPlayModeBeforeSaving));
            return false;
        }
        LevelObjectEditService& editing = m_context.GetLevelObjectEditService();
        if (editing.IsEditingLevelObject())
        {
            return editing.SaveLevelObjectEditor();
        }
        if (!m_level)
        {
            return false;
        }
        bool saved = Level::LevelSerializer::SaveLevel(*m_level, Level::SaveScope::All);
        if (saved)
        {
            Core::Log::Info(Text(EditorText::SavedLevel, m_level->GetName()));
        }
        else
        {
            Core::Log::Error(Text(EditorText::SaveLevelFailed, m_level->GetName()));
        }
        return saved;
    }

    bool LevelService::IsDirty() const
    {
        const LevelObjectEditService& editing = m_context.GetLevelObjectEditService();
        if (editing.IsEditingLevelObject())
        {
            return editing.IsDirty();
        }
        if (!m_level)
        {
            return false;
        }
        for (const auto& collection : m_level->GetCollections())
        {
            if (collection->IsDirty())
            {
                return true;
            }
        }
        return false;
    }

    void LevelService::MarkDirty(Level::LevelCollection* collection)
    {
        LevelObjectEditService& editing = m_context.GetLevelObjectEditService();
        if (editing.IsEditingLevelObject())
        {
            editing.MarkDirty();
        }
        if (collection)
        {
            collection->SetDirty(true);
        }
    }

    void LevelService::MarkDirty(const Level::LevelObject& object)
    {
        if (object.GetLevel() == m_level.get())
        {
            MarkDirty(object.GetCollection());
        }
    }

    Level::LevelCollection* LevelService::GetActiveCollection() const
    {
        Level::Level* level = GetActiveLevel();
        if (!level)
        {
            return nullptr;
        }
        if (m_activeCollection && GetCollectionIndex(m_activeCollection))
        {
            return m_activeCollection;
        }
        return level->GetDefaultCollection();
    }

    std::optional<size_t> LevelService::GetCollectionIndex(const Level::LevelCollection* collection) const
    {
        Level::Level* level = GetActiveLevel();
        if (!level || !collection)
        {
            return std::nullopt;
        }
        const auto& collections = level->GetCollections();
        for (size_t i = 0; i < collections.size(); ++i)
        {
            if (collections[i].get() == collection)
            {
                return i;
            }
        }
        return std::nullopt;
    }

    Level::LevelCollection* LevelService::GetCollectionByIndex(std::optional<size_t> index) const
    {
        Level::Level* level = GetActiveLevel();
        if (!level || !index || *index >= level->GetCollections().size())
        {
            return level ? level->GetDefaultCollection() : nullptr;
        }
        return level->GetCollections()[*index].get();
    }
}
