#pragma once

#include "Assets/AssetDatabase.h"
#include "Level/Component.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/ModelAsset.h"

#include <vector>

namespace BrightBox3D::Rendering
{
    BrightClass(Category = "Rendering")
    class MeshRenderer : public Level::Component
    {
        BrightBody()
    public:
        const Assets::AssetReference<ModelAsset>& GetModel() const { return m_model; }
        void SetModel(const Assets::AssetReference<ModelAsset>& model) { m_model = model; }
        int32_t GetMeshIndex() const { return m_meshIndex; }
        void SetMeshIndex(int32_t index) { m_meshIndex = index; }
        const std::vector<Assets::AssetReference<MaterialAsset>>& GetMaterials() const { return m_materials; }
        void SetMaterial(size_t slot, const Assets::AssetReference<MaterialAsset>& material);
        bool GetCastShadows() const { return m_castShadows; }
        void SetCastShadows(bool castShadows) { m_castShadows = castShadows; }

    private:
        BrightProperty(EditAnywhere, Category = "Mesh")
        Assets::AssetReference<ModelAsset> m_model;

        BrightProperty(EditAnywhere, Category = "Mesh")
        int32_t m_meshIndex = 0;

        BrightProperty(EditAnywhere, Category = "Materials")
        std::vector<Assets::AssetReference<MaterialAsset>> m_materials;

        BrightProperty(EditAnywhere, Category = "Lighting")
        bool m_castShadows = true;
    };
}
