#pragma once

#include "Reflection/EngineObject.h"

namespace BrightBox3D::Geometry
{
    BrightClass()
    struct ModelImportSettings : public Reflection::EngineObject
    {
        BrightBody()
    public:
        BrightProperty(.edit = Edit::Anywhere)
        float scale = 1.0f;

        BrightProperty(.edit = Edit::Anywhere)
        bool generateNormals = false;
    };
}
