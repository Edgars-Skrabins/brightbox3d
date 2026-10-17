#pragma once

#include "Assets/AssetImporter.h"

namespace BrightBox3D::Rendering
{
    class MaterialImporter : public Assets::JsonAssetImporter
    {
    protected:
        Core::Result<std::shared_ptr<Assets::Asset>> CreateAsset(Serialization::JsonDocument&& document) override;
    };
}
