#include "Framework/GameSession.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Framework/GameModeBase.h"
#include "Framework/PlayableLevelObject.h"
#include "Level/Level.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"

namespace BrightBox3D::Framework
{
    Reflection::EngineObject* GameSession::CreateFromSettings(Level::Level& level, GameplaySlot slot)
    {
        const Reflection::TypeInfo& base = LevelGameplaySettings::GetBaseType(slot);
        const Serialization::JsonValue* entry = LevelGameplaySettings::FindEntry(level.GetSettings(), slot);
        std::string className = entry ? LevelGameplaySettings::GetClassName(*entry) : std::string();
        if (className.empty())
        {
            if (LevelGameplaySettings::IsOptional(slot))
            {
                return nullptr;
            }
            className = base.GetName();
        }
        const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(className);
        if (!type || !type->IsA(base) || type->IsAbstract())
        {
            Core::Log::Warning(Core::Text(Core::EngineText::ClassNotLoaded, className, base.GetName()));
            if (LevelGameplaySettings::IsOptional(slot))
            {
                return nullptr;
            }
            type = &base;
        }
        Reflection::EngineObject* object = level.CreateEngineObject(*type);
        const Serialization::JsonValue* settings = entry ? LevelGameplaySettings::FindObjectSettings(*entry) : nullptr;
        if (object && settings)
        {
            Serialization::DeserializeProperties(*object, *settings);
        }
        return object;
    }

    GameModeBase* GameSession::Start(Level::Level& level)
    {
        level.GetRuntimeCollection();
        CreateFromSettings(level, GameplaySlot::LevelBehavior);
        auto* gameMode = Reflection::Cast<GameModeBase>(CreateFromSettings(level, GameplaySlot::GameMode));
        gameMode->CreateGameState();
        gameMode->CreatePlayerControllers();
        for (PlayerController* controller : gameMode->GetPlayerControllers())
        {
            gameMode->SpawnPlayer(*controller);
        }
        level.OnObjectDestroyed.Subscribe([](Level::LevelObject& object)
        {
            auto* playable = Reflection::Cast<PlayableLevelObject>(&object);
            if (PlayerController* controller = playable ? playable->GetController() : nullptr)
            {
                controller->HandlePlayableDestroyed();
            }
        });
        level.BeginPlay();
        Core::Log::Info(Core::Text(Core::EngineText::GameStarted, gameMode->GetType().GetName()));
        return gameMode;
    }
}
