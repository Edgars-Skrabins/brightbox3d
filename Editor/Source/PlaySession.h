#pragma once

#include "Core/Guid.h"
#include "Level/Level.h"

#include <memory>
#include <unordered_set>

namespace BrightBox3D::Editor
{
    class EditorContext;

    class PlaySession
    {
    public:
        explicit PlaySession(EditorContext& context);
        virtual ~PlaySession();

        virtual bool Start();
        virtual void Stop();
        virtual void Tick(float deltaTime);

        bool IsActive() const { return m_level != nullptr; }
        Level::Level* GetLevel() const { return m_level.get(); }
        bool IsPaused() const { return m_level && m_level->IsPaused(); }
        void SetPaused(bool paused);
        void Step();

        void ToggleKeepChanges(const Core::Guid& id);
        bool IsKeepingChanges(const Core::Guid& id) const { return m_keepChanges.count(id) > 0; }

    protected:
        virtual void BeginGame();
        void ApplyKeptChanges(Level::Level& playedLevel);

        EditorContext& m_context;
        std::unique_ptr<Level::Level> m_level;
        std::unordered_set<Core::Guid> m_keepChanges;
    };
}
