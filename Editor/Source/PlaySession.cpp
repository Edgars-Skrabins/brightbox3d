#include "PlaySession.h"

#include "Core/Log.h"
#include "EditorContext.h"
#include "EditorText.h"
#include "Level/LevelSerializer.h"

namespace BrightBox3D::Editor
{
    PlaySession::PlaySession(EditorContext& context) : m_context(context)
    {
    }

    PlaySession::~PlaySession()
    {
        if (IsActive())
        {
            Stop();
        }
    }

    bool PlaySession::Start()
    {
        Level::Level* edit = m_context.GetLevelService().GetEditLevel();
        if (!edit || IsActive())
        {
            return false;
        }
        m_keepChanges.clear();
        m_level = Level::LevelSerializer::CloneLevel(*edit);
        m_level->GetSettings().CopyFrom(edit->GetSettings(), m_level->GetSettings().GetAllocator());
        BeginGame();
        Core::Log::Info(Text(EditorText::PlayModeStarted));
        return true;
    }

    void PlaySession::BeginGame()
    {
        m_level->BeginPlay();
    }

    void PlaySession::Stop()
    {
        if (!IsActive())
        {
            return;
        }
        m_level->EndPlay();
        std::unique_ptr<Level::Level> played = std::move(m_level);
        ApplyKeptChanges(*played);
        played.reset();
        m_keepChanges.clear();
        m_context.GetLevelService().BumpLevelRevision();
        Core::Log::Info(Text(EditorText::PlayModeStopped));
    }

    void PlaySession::Tick(float deltaTime)
    {
        if (IsActive())
        {
            m_level->Update(deltaTime);
        }
    }

    void PlaySession::SetPaused(bool paused)
    {
        if (m_level)
        {
            m_level->SetPaused(paused);
        }
    }

    void PlaySession::Step()
    {
        if (m_level)
        {
            m_level->Step(m_level->GetFixedDeltaTime());
        }
    }

    void PlaySession::ToggleKeepChanges(const Core::Guid& id)
    {
        if (m_keepChanges.count(id))
        {
            m_keepChanges.erase(id);
        }
        else
        {
            m_keepChanges.insert(id);
        }
    }

    void PlaySession::ApplyKeptChanges(Level::Level& playedLevel)
    {
        Level::Level* edit = m_context.GetLevelService().GetEditLevel();
        if (!edit)
        {
            return;
        }
        for (const Core::Guid& id : m_keepChanges)
        {
            Level::LevelObject* played = playedLevel.FindObject(id);
            Level::LevelObject* original = edit->FindObject(id);
            if (!played || !original)
            {
                Core::Log::Warning(Text(EditorText::KeepChangesMissingObject));
                continue;
            }
            Serialization::JsonDocument before;
            ObjectEditService::Snapshot(*original, before);
            Serialization::JsonDocument after;
            ObjectEditService::Snapshot(*played, after);
            Core::Guid parent = original->GetParent() ? original->GetParent()->GetId() : Core::Guid{};
            int collection = m_context.GetLevelService().GetCollectionIndex(original->GetCollection());
            int sibling = original->GetSiblingIndex();
            m_context.GetObjectEditService().RestoreSnapshot(after, parent, collection, sibling);
            m_context.GetUndoService().RecordObjectChange(id, before, Text(EditorText::UndoKeepPlayModeChanges));
            Core::Log::Info(Text(EditorText::KeptPlayModeChanges, played->GetName()));
        }
    }
}
