#pragma once

#include "Level/LevelObjectInstance.h"

BrightClass(Category = "Arena")
class ArenaPlayer : public BrightBox3D::Level::LevelObjectInstance
{
    BrightBody()
public:
    void OnGameStart() override;
    void OnGameUpdate(float deltaTime) override;

private:
    BrightProperty(EditDefaultsOnly, Category = "Movement")
    float m_moveSpeed = 6.0f;

    BrightProperty(EditDefaultsOnly, Category = "Movement")
    float m_lookSensitivity = 0.15f;

    BrightProperty(VisibleAnywhere, Category = "Stats")
    BrightUIProperty(Read)
    int m_health = 100;

    BrightUIProperty(Read)
    float m_maxHealth = 100.0f;

    BrightUIProperty(Write)
    std::string m_playerName = "Arena Hero";
};
