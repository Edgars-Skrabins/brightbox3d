#pragma once

#include "Framework/GameModeBase.h"
#include "Framework/GameStateBase.h"
#include "Framework/PlayableLevelObject.h"
#include "Level/Level.h"
#include "Level/LevelBehavior.h"

#include <string>
#include <vector>

namespace BrightBox3D::Tests
{
    std::vector<std::string>& FrameworkLog();

    BrightClass()
    class TestPlayable : public Framework::PlayableLevelObject
    {
        BrightBody()
    public:
        void OnPossessed(Framework::PlayerController& controller) override { ++m_possessions; }
        void OnUnpossessed() override { ++m_unpossessions; }

        int m_possessions = 0;
        int m_unpossessions = 0;
    };

    BrightClass()
    class RecordingGameState : public Framework::GameStateBase
    {
        BrightBody()
    public:
        void OnGameLoad() override { FrameworkLog().push_back("GameState:Load"); }
        void OnGameStart() override { FrameworkLog().push_back("GameState:Start"); }
        void OnGameEnd() override { FrameworkLog().push_back("GameState:End"); }

        BrightProperty(.edit = Edit::Anywhere)
        int m_value = 0;
    };

    BrightClass()
    class RecordingLevelBehavior : public Level::LevelBehavior
    {
        BrightBody()
    public:
        void OnGameLoad() override { FrameworkLog().push_back("LevelBehavior:Load"); }
        void OnGameStart() override { FrameworkLog().push_back("LevelBehavior:Start"); }
        void OnGameEnd() override { FrameworkLog().push_back("LevelBehavior:End"); }
    };

    BrightClass()
    class RecordingGameMode : public Framework::GameModeBase
    {
        BrightBody()
    public:
        RecordingGameMode() { m_gameStateClass.Set<RecordingGameState>(); }

        void OnGameLoad() override { FrameworkLog().push_back("GameMode:Load"); }
        void OnGameStart() override { FrameworkLog().push_back("GameMode:Start"); }
        void OnGameEnd() override { FrameworkLog().push_back("GameMode:End"); }

        void SpawnPlayer(Framework::PlayerController& controller) override
        {
            ++m_spawns;
            auto* playable = *GetLevel()->CreateObject<TestPlayable>();
            controller.Possess(*playable);
        }

        int m_spawns = 0;
    };
}
