#pragma once

#include "Assets/AssetDatabase.h"
#include "Level/LevelAssets.h"
#include "Level/LevelObjectInstance.h"

#include <vector>

namespace BrightBox3D::Framework
{
    class PlayerStart;

    BrightClass(Category = "Gameplay")
    class GameMode : public Level::LevelObjectInstance
    {
        BrightBody()
    public:
        void OnGameStart() override;

        virtual Level::LevelObjectInstance* SpawnPlayer(int32_t playerIndex);
        virtual PlayerStart* ChoosePlayerStart(int32_t playerIndex);
        virtual void OnPlayerSpawned(Level::LevelObjectInstance& player, int32_t playerIndex) {}

        Level::LevelObjectInstance* GetPlayer(int32_t playerIndex) const;
        const std::vector<Core::Guid>& GetPlayers() const { return m_players; }
        const Assets::AssetReference<Level::LevelObjectAsset>& GetDefaultPlayer() const { return m_defaultPlayer; }
        void SetDefaultPlayer(const Assets::AssetReference<Level::LevelObjectAsset>& player) { m_defaultPlayer = player; }

        static GameMode* Get(const Level::Level& level);

    protected:
        BrightProperty(EditDefaultsOnly, Category = "Players")
        Assets::AssetReference<Level::LevelObjectAsset> m_defaultPlayer;

        BrightProperty(EditDefaultsOnly, Category = "Players")
        bool m_spawnPlayerOnStart = true;

        BrightProperty(EditDefaultsOnly, Category = "Players")
        int32_t m_playerCount = 1;

        std::vector<Core::Guid> m_players;
    };
}
