#pragma once

#include "Assets/Asset.h"
#include "Rendering/MeshData.h"

#include <vector>

namespace BrightBox3D::Rendering
{
    BrightClass()
    class ModelAsset : public Assets::Asset
    {
        BrightBody()
    public:
        static constexpr const char* AssetTypeName = "Model";

        std::vector<MeshData>& GetMeshes() { return m_meshes; }
        const std::vector<MeshData>& GetMeshes() const { return m_meshes; }
        std::vector<ModelNode>& GetNodes() { return m_nodes; }
        const std::vector<ModelNode>& GetNodes() const { return m_nodes; }
        std::vector<EmbeddedMaterial>& GetMaterials() { return m_materials; }
        const std::vector<EmbeddedMaterial>& GetMaterials() const { return m_materials; }
        const MeshData* GetMesh(int index) const { return index >= 0 && index < static_cast<int>(m_meshes.size()) ? &m_meshes[static_cast<size_t>(index)] : nullptr; }

        std::vector<uint8_t> Serialize() const;
        bool Deserialize(const uint8_t* data, size_t size);

    private:
        std::vector<MeshData> m_meshes;
        std::vector<ModelNode> m_nodes;
        std::vector<EmbeddedMaterial> m_materials;
    };
}
