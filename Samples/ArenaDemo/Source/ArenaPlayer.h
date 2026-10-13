#pragma once

#include "ArenaCategories.h"
#include "Framework/PlayableLevelObject.h"

BrightClass(.category = Arena)
class ArenaPlayer : public BrightBox3D::Framework::PlayableLevelObject
{
    BrightBody()
public:
    void OnGameStart() override;
    void OnGameUpdate(float deltaTime) override;

private:
    BrightProperty(.edit = Edit::DefaultsOnly, .category = Movement)
    float m_moveSpeed = 6.0f;

    BrightProperty(.edit = Edit::DefaultsOnly, .category = Movement)
    float m_lookSensitivity = 0.15f;

    BrightProperty(.edit = Edit::Visible, .category = Stats)
    BrightUIProperty(.access = UIAccess::Read)
    int m_health = 100;

    BrightUIProperty(.access = UIAccess::Read)
    float m_maxHealth = 100.0f;

    BrightUIProperty(.access = UIAccess::Write)
    std::string m_playerName = "Arena Hero";
};
