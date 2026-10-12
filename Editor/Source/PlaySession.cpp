#include "PlaySession.h"

#include "Core/Log.h"
#include "EditorContext.h"
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
        Level::Level* edit = m_context.GetEditLevel();
        if (!edit || IsActive())
        {
            return false;
        }
        m_keepChanges.clear();
        m_level = Level::LevelSerializer::CloneLevel(*edit);
        m_level->GetSettings().CopyFrom(edit->GetSettings(), m_level->GetSettings().GetAllocator());
        BeginGame();
        Core::Log::Info("PlayMode started");
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
        m_context.BumpLevelRevision();
        Core::Log::Info("PlayMode stopped");
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
        Level::Level* edit = m_context.GetEditLevel();
        if (!edit)
        {
            return;
        }
        for (const Core::Guid& id : m_keepChanges)
        {
            Level::LevelObjectInstance* played = playedLevel.FindObject(id);
            Level::LevelObjectInstance* original = edit->FindObject(id);
            if (!played || !original)
            {
                Core::Log::Warning("Keep changes: object no longer exists in both levels");
                continue;
            }
            Serialization::JsonDocument before;
            EditorContext::Snapshot(*original, before);
            Serialization::JsonDocument after;
            EditorContext::Snapshot(*played, after);
            Core::Guid parent = original->GetParent() ? original->GetParent()->GetId() : Core::Guid{};
            int collection = m_context.GetCollectionIndex(original->GetCollection());
            int sibling = original->GetSiblingIndex();
            m_context.RestoreSnapshot(after, parent, collection, sibling);
            m_context.RecordObjectChange(id, before, "Keep PlayMode Changes");
            Core::Log::Info("Kept PlayMode changes for '" + played->GetName() + "'");
        }
    }
}
