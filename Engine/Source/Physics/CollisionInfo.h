#pragma once

#include "Math/Vector3.h"

namespace BrightBox3D::Level
{
    class LevelObject;
}

namespace BrightBox3D::Physics
{
    struct CollisionInfo
    {
        Level::LevelObject* other = nullptr;
        Math::Vector3 point;
        Math::Vector3 normal;
        Math::Vector3 relativeVelocity;
        bool isTrigger = false;
    };
}
