#include "Framework/GameSession.h"

#include "Core/Log.h"
#include "Framework/GameMode.h"
#include "Level/Level.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"

namespace BrightBox3D::Framework
{
    const Serialization::JsonValue* GameSession::ResolveGameModeSettings(const Level::Level& level, const Serialization::JsonValue& projectGameMode)
    {
        const Serialization::JsonValue* settings = &projectGameMode;
        if (Serialization::GetBool(level.GetSettings(), "overrideGameMode", false))
        {
            if (const Serialization::JsonValue* levelGameMode = Serialization::FindMember(level.GetSettings(), "gameMode"); levelGameMode && levelGameMode->IsObject())
            {
                settings = levelGameMode;
            }
        }
        return settings;
    }

    GameMode* GameSession::Start(Level::Level& level, const Serialization::JsonValue& projectGameMode)
    {
        const Serialization::JsonValue* settings = ResolveGameModeSettings(level, projectGameMode);
        std::string typeName = Serialization::GetString(*settings, Serialization::TypeKey, "GameMode");
        const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(typeName);
        if (!type || !type->IsA(GameMode::StaticType()) || type->IsAbstract())
        {
            Core::Log::Warning("GameMode type '" + typeName + "' was not found; using the default GameMode");
            type = &GameMode::StaticType();
        }
        Level::LevelCollection* runtime = level.FindCollectionByName("Runtime");
        if (!runtime)
        {
            runtime = level.AddCollection("Runtime");
        }
        auto* gameMode = static_cast<GameMode*>(level.CreateObject(*type, runtime, nullptr, {}, false));
        gameMode->SetName("GameMode");
        gameMode->SetHideInLevelContents(true);
        Serialization::DeserializeProperties(*gameMode, *settings);
        level.InitializeObject(*gameMode);
        level.BeginPlay();
        Core::Log::Info("Game started with GameMode '" + type->GetName() + "'");
        return gameMode;
    }
}
