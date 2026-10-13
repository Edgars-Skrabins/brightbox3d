#pragma once

#include "Framework/GameMode.h"

BrightClass(Category = "Arena")
class ArenaGameMode : public BrightBox3D::Framework::GameMode
{
    BrightBody()
public:
    void OnGameUpdate(float deltaTime) override;

private:
    BrightProperty(EditDefaultsOnly, Category = "Rules")
    float m_roundLength = 120.0f;

    BrightUIProperty(Read)
    float m_timeRemaining = 120.0f;

    BrightUIProperty(Write)
    bool m_restartRequested = false;
};
