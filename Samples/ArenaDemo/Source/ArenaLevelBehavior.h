#pragma once

#include "ArenaCategories.h"
#include "Level/LevelBehavior.h"

BrightClass(.category = Arena)
class ArenaLevelBehavior : public BrightBox3D::Level::LevelBehavior
{
    BrightBody()
public:
    void OnGameStart() override;
};
