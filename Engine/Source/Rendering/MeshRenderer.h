#pragma once

#include "Assets/AssetDatabase.h"
#include "Geometry/Categories.h"
#include "Geometry/MeshSource.h"
#include "Geometry/ModelAsset.h"
#include "Level/LevelObjectComponent.h"
#include "Rendering/Categories.h"
#include "Rendering/MaterialAsset.h"

#include <vector>

namespace BrightBox3D::Rendering
{
    BrightClass(.category = Categories::Rendering, .icon = Icon::Box)
    class MeshRenderer : public Level::LevelObjectComponent, public Geometry::MeshSource
    {
        BrightBody()
    public:
        const Assets::AssetReference<Geometry::ModelAsset>& GetModel() const override { return m_model; }
        void SetModel(const Assets::AssetReference<Geometry::ModelAsset>& model) { m_model = model; }
        int32_t GetMeshIndex() const override { return m_meshIndex; }
        void SetMeshIndex(int32_t index) { m_meshIndex = index; }
        const std::vector<Assets::AssetReference<MaterialAsset>>& GetMaterials() const { return m_materials; }
        void SetMaterial(size_t slot, const Assets::AssetReference<MaterialAsset>& material);
        bool GetCastShadows() const { return m_castShadows; }
        void SetCastShadows(bool castShadows) { m_castShadows = castShadows; }

    private:
        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Mesh)
        Assets::AssetReference<Geometry::ModelAsset> m_model;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Mesh)
        int32_t m_meshIndex = 0;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Materials)
        std::vector<Assets::AssetReference<MaterialAsset>> m_materials;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Lighting)
        bool m_castShadows = true;
    };
}
