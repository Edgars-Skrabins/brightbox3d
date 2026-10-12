#pragma once

#include "Serialization/Json.h"

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Framework
{
    class GameMode;

    class GameSession
    {
    public:
        static GameMode* Start(Level::Level& level, const Serialization::JsonValue& projectGameMode);
        static const Serialization::JsonValue* ResolveGameModeSettings(const Level::Level& level, const Serialization::JsonValue& projectGameMode);
    };
}
