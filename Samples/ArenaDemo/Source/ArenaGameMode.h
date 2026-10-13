#pragma once

#include "ArenaCategories.h"
#include "Framework/GameModeBase.h"

BrightClass(.category = Arena)
class ArenaGameMode : public BrightBox3D::Framework::GameModeBase
{
    BrightBody()
public:
    ArenaGameMode();

    void OnGameUpdate(float deltaTime) override;

private:
    BrightProperty(.edit = Edit::DefaultsOnly, .category = Rules)
    float m_roundLength = 120.0f;

    BrightUIProperty(.access = UIAccess::Read)
    float m_timeRemaining = 120.0f;

    BrightUIProperty(.access = UIAccess::Write)
    bool m_restartRequested = false;
};
