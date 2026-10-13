#pragma once

#include "Framework/Categories.h"
#include "Level/Categories.h"
#include "Level/LevelObject.h"

namespace BrightBox3D::Framework
{
    BrightClass(.category = Categories::Gameplay, .icon = Icon::Flag)
    class PlayerStart : public Level::LevelObject
    {
        BrightBody()
    public:
        int32_t GetPlayerIndex() const { return m_playerIndex; }

    private:
        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Spawn)
        int32_t m_playerIndex = -1;
    };
}
