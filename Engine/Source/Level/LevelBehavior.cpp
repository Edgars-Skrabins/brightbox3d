#include "Level/LevelBehavior.h"

#include "Level/Level.h"

namespace BrightBox3D::Level
{
    LevelBehavior* LevelBehavior::Get(const Level& level)
    {
        return level.FindEngineObjectOfType<LevelBehavior>();
    }
}
