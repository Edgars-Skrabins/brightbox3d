#pragma once

#include "Core/Guid.h"
#include "Level/Level.h"

#include <memory>
#include <string>

namespace BrightBox3D::Editor
{
    class EditorContext;
    class PlaySession;

    class LevelService
    {
    public:
        explicit LevelService(EditorContext& context) : m_context(context) {}

        Level::Level* GetEditLevel() const { return m_level.get(); }
        Level::Level* GetActiveLevel() const;
        std::unique_ptr<Level::Level> ExchangeEditLevel(std::unique_ptr<Level::Level> level);
        bool OpenLevel(const Core::Guid& levelAsset);
        void OpenStartupLevel();
        bool NewLevel(const std::string& relativePath);
        bool SaveLevel();
        bool IsDirty() const;
        void MarkDirty(Level::LevelCollection* collection);
        void MarkDirty(const Level::LevelObject& object);
        void Reset();

        Level::LevelCollection* GetActiveCollection() const;
        void SetActiveCollection(Level::LevelCollection* collection) { m_activeCollection = collection; }
        int GetCollectionIndex(const Level::LevelCollection* collection) const;
        Level::LevelCollection* GetCollectionByIndex(int index) const;

        PlaySession* GetPlaySession() const { return m_playSession; }
        void SetPlaySession(PlaySession* session) { m_playSession = session; }
        bool IsPlaying() const;

        uint64_t GetLevelRevision() const { return m_levelRevision; }
        void BumpLevelRevision() { ++m_levelRevision; }

    private:
        EditorContext& m_context;
        std::unique_ptr<Level::Level> m_level;
        Level::LevelCollection* m_activeCollection = nullptr;
        PlaySession* m_playSession = nullptr;
        uint64_t m_levelRevision = 0;
    };
}
