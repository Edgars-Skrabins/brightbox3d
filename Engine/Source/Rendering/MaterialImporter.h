#pragma once

#include "Assets/AssetImporter.h"

namespace BrightBox3D::Rendering
{
    class MaterialImporter : public Assets::JsonAssetImporter
    {
    protected:
        std::shared_ptr<Assets::Asset> CreateAsset(Serialization::JsonDocument&& document, std::string& error) override;
    };
}
