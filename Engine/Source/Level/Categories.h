#pragma once

#include "Reflection/Metadata.h"

namespace BrightBox3D::Reflection::Categories
{
    inline constexpr Category Gameplay{ "Gameplay" };
    inline constexpr Category Transform{ "Transform" };
}

namespace BrightBox3D::Reflection::DisplayNames
{
    inline constexpr DisplayName Position{ "Position" };
    inline constexpr DisplayName Rotation{ "Rotation" };
    inline constexpr DisplayName Scale{ "Scale" };
}
