#pragma once

#include "Level/LevelObjectInstance.h"

#include <string>

namespace BrightBox3D::Tests
{
    BrightClass()
    class UITestPlayer : public Level::LevelObjectInstance
    {
        BrightBody()
    public:
        BrightUIProperty(Read)
        int m_health = 75;

        BrightUIProperty(Read)
        float m_maxHealth = 100.0f;

        BrightUIProperty(Write)
        float m_volume = 0.25f;

        BrightUIProperty(Write)
        bool m_leaveRequested = false;

        BrightUIProperty(Write)
        std::string m_playerName = "Bright";

        BrightProperty(EditAnywhere)
        int m_notUI = 1;
    };
}
