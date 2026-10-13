#pragma once

#include "Core/Guid.h"
#include "Math/Geometry.h"

namespace BrightBox3D::Level
{
    class Level;
    class LevelObject;
}

namespace BrightBox3D::Editor
{
    struct PickResult
    {
        Level::LevelObject* object = nullptr;
        float distance = 0.0f;
        Math::Vector3 point;
    };

    PickResult PickObject(const Level::Level& level, const Math::Ray& ray);
}
