#pragma once

#include "Level/Categories.h"
#include "Reflection/EngineObject.h"

namespace BrightBox3D::Level
{
    class Level;

    BrightClass(.category = Categories::Gameplay)
    class LevelBehavior : public Reflection::EngineObject
    {
        BrightBody()
    public:
        static LevelBehavior* Get(const Level& level);
    };
}
