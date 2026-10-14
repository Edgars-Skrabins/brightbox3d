#pragma once

#include "Reflection/EngineObject.h"
#include "Rendering/TextureAsset.h"

namespace BrightBox3D::Rendering
{
    BrightClass()
    struct TextureImportSettings : public Reflection::EngineObject
    {
        BrightBody()
    public:
        BrightProperty(.edit = Edit::Anywhere)
        bool srgb = true;

        BrightProperty(.edit = Edit::Anywhere)
        bool normalMap = false;

        BrightProperty(.edit = Edit::Anywhere)
        bool generateMips = true;

        BrightProperty(.edit = Edit::Anywhere)
        TextureFilterMode filter = TextureFilterMode::Trilinear;

        BrightProperty(.edit = Edit::Anywhere)
        TextureWrapMode wrap = TextureWrapMode::Repeat;
    };
}
