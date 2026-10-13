#pragma once

#include "Framework/LevelGameplaySettings.h"

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Reflection
{
    class EngineObject;
}

namespace BrightBox3D::Framework
{
    class GameModeBase;

    class GameSession
    {
    public:
        static GameModeBase* Start(Level::Level& level);

    private:
        static Reflection::EngineObject* CreateFromSettings(Level::Level& level, GameplaySlot slot);
    };
}
