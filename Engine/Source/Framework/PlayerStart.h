#pragma once

#include "Level/LevelObjectInstance.h"

namespace BrightBox3D::Framework
{
    BrightClass(Category = "Gameplay")
    class PlayerStart : public Level::LevelObjectInstance
    {
        BrightBody()
    public:
        int32_t GetPlayerIndex() const { return m_playerIndex; }

    private:
        BrightProperty(EditAnywhere, Category = "Spawn")
        int32_t m_playerIndex = -1;
    };
}
