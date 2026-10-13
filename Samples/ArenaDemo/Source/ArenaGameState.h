#pragma once

#include "ArenaCategories.h"
#include "Framework/GameStateBase.h"

BrightClass(.category = Arena)
class ArenaGameState : public BrightBox3D::Framework::GameStateBase
{
    BrightBody()
public:
    void OnGameUpdate(float deltaTime) override;
    void ResetScore();

private:
    BrightUIProperty(.access = UIAccess::Read)
    int m_score = 0;

    float m_elapsed = 0.0f;
};
