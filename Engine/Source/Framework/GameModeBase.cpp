#include "Framework/GameModeBase.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Framework/PlayableLevelObject.h"
#include "Framework/PlayerStart.h"
#include "Level/Level.h"
#include "Reflection/TypeRedirects.h"


namespace BrightBox3D::Framework
{
    namespace
    {
        Reflection::TypeRedirectRegistration<GameModeBase> s_gameModeRedirect("GameMode");

    }

    void GameModeBase::CreateGameState()
    {
        Level::Level* level = GetLevel();
        const Reflection::TypeInfo* type = m_gameStateClass.Resolve();
        if (!type)
        {
            Core::Log::Warning(Core::Text(Core::EngineText::ClassNotLoadedFallback, GameStateBase::StaticType().GetName(), m_gameStateClass.typeName, GameStateBase::StaticType().GetName()));
            type = &GameStateBase::StaticType();
        }
        m_gameState = Reflection::Cast<GameStateBase>(level->CreateEngineObject(*type));
        if (m_gameState)
        {
            m_gameState->m_gameMode = this;
        }
    }

    void GameModeBase::CreatePlayerControllers()
    {
        Level::Level* level = GetLevel();
        const Reflection::TypeInfo* type = m_playerControllerClass.Resolve();
        if (!type)
        {
            Core::Log::Warning(Core::Text(Core::EngineText::ClassNotLoadedFallback, PlayerController::StaticType().GetName(), m_playerControllerClass.typeName, PlayerController::StaticType().GetName()));
            type = &PlayerController::StaticType();
        }
        for (int32_t index = 0; index < std::max(m_playerCount, 1); ++index)
        {
            if (auto* controller = Reflection::Cast<PlayerController>(level->CreateEngineObject(*type)))
            {
                controller->m_playerIndex = index;
                m_controllers.push_back(controller);
            }
        }
    }

    PlayerStart* GameModeBase::ChoosePlayerStart(const PlayerController& controller)
    {
        Level::Level* level = GetLevel();
        std::vector<PlayerStart*> starts = level->FindObjectsOfType<PlayerStart>();
        for (PlayerStart* start : starts)
        {
            if (start->GetPlayerIndex() == controller.GetPlayerIndex())
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

    void GameModeBase::SpawnPlayer(PlayerController& controller)
    {
        Level::Level* level = GetLevel();
        if (!m_defaultPlayable.IsSet())
        {
            Core::Log::Warning(Core::Text(Core::EngineText::NoDefaultPlayable, controller.GetPlayerIndex()));
            return;
        }
        PlayerStart* start = ChoosePlayerStart(controller);
        Math::Vector3 position = start ? start->GetTransform().GetPosition() : Math::Vector3();
        Math::Quaternion rotation = start ? start->GetTransform().GetRotation() : Math::Quaternion();
        Level::LevelObject* spawned = level->Spawn(m_defaultPlayable, position, rotation);
        auto* playable = Reflection::Cast<PlayableLevelObject>(spawned);
        if (!playable)
        {
            Core::Log::Error(Core::Text(Core::EngineText::DefaultPlayableWrongType, Assets::AssetTypeName(Assets::AssetType::LevelObjectVariant), PlayableLevelObject::StaticType().GetName()));
            if (spawned)
            {
                level->DestroyObjectImmediate(spawned);
            }
            return;
        }
        controller.Possess(*playable);
    }

    void GameModeBase::RestartPlayer(PlayerController& controller)
    {
        if (PlayableLevelObject* playable = controller.GetPlayable())
        {
            controller.UnPossess();
            playable->Destroy();
        }
        SpawnPlayer(controller);
    }

    void GameModeBase::OnPlayableDestroyed(PlayerController& controller)
    {
        SpawnPlayer(controller);
    }

    PlayerController* GameModeBase::GetPlayerController(int32_t playerIndex) const
    {
        return playerIndex >= 0 && playerIndex < static_cast<int32_t>(m_controllers.size()) ? m_controllers[static_cast<size_t>(playerIndex)] : nullptr;
    }

    void GameModeBase::CarryReferences(Reflection::ReferenceCarrier& carrier)
    {
        carrier.Carry(m_gameState);
        carrier.CarryAll(m_controllers);
    }

    GameModeBase* GameModeBase::Get(const Level::Level& level)
    {
        return level.FindEngineObjectOfType<GameModeBase>();
    }
}
