#pragma once

#include "Level/LevelObject.h"

#include <string>

namespace BrightBox3D::Tests
{
    BrightClass()
    class UITestPlayer : public Level::LevelObject
    {
        BrightBody()
    public:
        BrightUIProperty(.access = UIAccess::Read)
        int m_health = 75;

        BrightUIProperty(.access = UIAccess::Read)
        float m_maxHealth = 100.0f;

        BrightUIProperty(.access = UIAccess::Write)
        float m_volume = 0.25f;

        BrightUIProperty(.access = UIAccess::Write)
        bool m_leaveRequested = false;

        BrightUIProperty(.access = UIAccess::Write)
        std::string m_playerName = "Bright";

        BrightProperty(.edit = Edit::Anywhere)
        int m_notUI = 1;
    };
}
