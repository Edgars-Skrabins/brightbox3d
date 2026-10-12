#include "Framework/GameMode.h"

#include "Core/Log.h"
#include "Framework/PlayerController.h"
#include "Framework/PlayerStart.h"
#include "Level/Level.h"
#include "Level/LevelSerializer.h"

namespace BrightBox3D::Framework
{
    void GameMode::OnGameStart()
    {
        if (!m_spawnPlayerOnStart)
        {
            return;
        }
        for (int32_t index = 0; index < std::max(m_playerCount, 1); ++index)
        {
            SpawnPlayer(index);
        }
    }

    PlayerStart* GameMode::ChoosePlayerStart(int32_t playerIndex)
    {
        Level::Level* level = GetLevel();
        if (!level)
        {
            return nullptr;
        }
        std::vector<PlayerStart*> starts = level->FindObjectsOfType<PlayerStart>();
        for (PlayerStart* start : starts)
        {
            if (start->GetPlayerIndex() == playerIndex)
            {
                return start;
            }
        }
        for (PlayerStart* start : starts)
        {
            if (start->GetPlayerIndex() < 0)
            {
                return start;
            }
        }
        return starts.empty() ? nullptr : starts.front();
    }

    Level::LevelObjectInstance* GameMode::SpawnPlayer(int32_t playerIndex)
    {
        Level::Level* level = GetLevel();
        if (!level)
        {
            return nullptr;
        }
        if (!m_defaultPlayer.IsSet())
        {
            Core::Log::Warning("GameMode has no DefaultPlayer; no player was spawned");
            return nullptr;
        }
        PlayerStart* start = ChoosePlayerStart(playerIndex);
        Level::LevelObjectInstance* player = Level::LevelSerializer::Instantiate(*level, m_defaultPlayer.guid, level->FindCollectionByName("Runtime"), nullptr, {}, false);
        if (!player)
        {
            Core::Log::Error("GameMode could not spawn the DefaultPlayer LevelObject");
            return nullptr;
        }
        if (start)
        {
            player->GetTransform().SetPositionAndRotation(start->GetTransform().GetPosition(), start->GetTransform().GetRotation());
        }
        if (PlayerController* controller = player->GetComponent<PlayerController>())
        {
            controller->SetPlayerIndex(playerIndex);
        }
        std::function<void(Level::LevelObjectInstance&)> initialize = [&](Level::LevelObjectInstance& object)
        {
            level->InitializeObject(object);
            std::vector<Level::LevelObjectInstance*> children = object.GetChildren();
            for (Level::LevelObjectInstance* child : children)
            {
                initialize(*child);
            }
        };
        initialize(*player);
        if (static_cast<int32_t>(m_players.size()) <= playerIndex)
        {
            m_players.resize(static_cast<size_t>(playerIndex) + 1);
        }
        m_players[static_cast<size_t>(playerIndex)] = player->GetId();
        OnPlayerSpawned(*player, playerIndex);
        return player;
    }

    Level::LevelObjectInstance* GameMode::GetPlayer(int32_t playerIndex) const
    {
        Level::Level* level = GetLevel();
        if (!level || playerIndex < 0 || playerIndex >= static_cast<int32_t>(m_players.size()))
        {
            return nullptr;
        }
        return level->FindObject(m_players[static_cast<size_t>(playerIndex)]);
    }

    GameMode* GameMode::Get(const Level::Level& level)
    {
        return level.FindObjectOfType<GameMode>();
    }
}
