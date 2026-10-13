#pragma once

#include "Assets/AssetDatabase.h"
#include "Framework/Categories.h"
#include "Framework/GameStateBase.h"
#include "Framework/PlayerController.h"
#include "Level/Categories.h"
#include "Level/LevelAssets.h"
#include "Reflection/ClassReference.h"
#include "Reflection/EngineObject.h"

#include <vector>

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Framework
{
    class PlayableLevelObject;
    class PlayerStart;

    BrightClass(.category = Categories::Gameplay)
    class GameModeBase : public Reflection::EngineObject
    {
        BrightBody()
    public:
        virtual void SpawnPlayer(PlayerController& controller);
        virtual void RestartPlayer(PlayerController& controller);
        virtual void OnPlayableDestroyed(PlayerController& controller);
        virtual PlayerStart* ChoosePlayerStart(const PlayerController& controller);

        GameStateBase* GetGameState() const { return m_gameState; }
        const std::vector<PlayerController*>& GetPlayerControllers() const { return m_controllers; }
        PlayerController* GetPlayerController(int32_t playerIndex) const;
        int32_t GetPlayerCount() const { return m_playerCount; }
        const Assets::AssetReference<Level::LevelObjectVariantAsset>& GetDefaultPlayable() const { return m_defaultPlayable; }
        void SetDefaultPlayable(const Assets::AssetReference<Level::LevelObjectVariantAsset>& playable) { m_defaultPlayable = playable; }

        void CarryReferences(Reflection::ReferenceCarrier& carrier) override;

        static GameModeBase* Get(const Level::Level& level);

    protected:
        BrightProperty(.edit = Edit::DefaultsOnly, .category = Categories::Classes)
        Reflection::ClassReference<GameStateBase> m_gameStateClass;

        BrightProperty(.edit = Edit::DefaultsOnly, .category = Categories::Classes)
        Reflection::ClassReference<PlayerController> m_playerControllerClass;

        BrightProperty(.edit = Edit::DefaultsOnly, .category = Categories::Players)
        Assets::AssetReference<Level::LevelObjectVariantAsset> m_defaultPlayable;

        BrightProperty(.edit = Edit::DefaultsOnly, .category = Categories::Players)
        int32_t m_playerCount = 1;

    private:
        friend class GameSession;

        void CreateGameState();
        void CreatePlayerControllers();

        GameStateBase* m_gameState = nullptr;
        std::vector<PlayerController*> m_controllers;
    };
}
