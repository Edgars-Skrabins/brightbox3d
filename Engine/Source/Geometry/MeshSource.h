#pragma once

#include "Assets/AssetDatabase.h"
#include "Geometry/ModelAsset.h"

#include <cstdint>

namespace BrightBox3D::Geometry
{
    class MeshSource
    {
    public:
        virtual ~MeshSource() = default;

        virtual const Assets::AssetReference<ModelAsset>& GetModel() const = 0;
        virtual int32_t GetMeshIndex() const = 0;
    };
}
