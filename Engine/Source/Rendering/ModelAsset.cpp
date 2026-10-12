#include "Rendering/ModelAsset.h"

#include "Serialization/BinaryStream.h"

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr uint32_t ModelFormatVersion = 1;
    }

    std::vector<uint8_t> ModelAsset::Serialize() const
    {
        Serialization::BinaryWriter writer;
        writer.Write(ModelFormatVersion);
        writer.WriteVarUInt(m_meshes.size());
        for (const MeshData& mesh : m_meshes)
        {
            writer.WriteString(mesh.name);
            writer.WriteVector(mesh.vertices);
            writer.WriteVector(mesh.indices);
            writer.WriteVector(mesh.subMeshes);
            writer.Write(mesh.bounds);
        }
        writer.WriteVarUInt(m_nodes.size());
        for (const ModelNode& node : m_nodes)
        {
            writer.WriteString(node.name);
            writer.Write(node.parent);
            writer.Write(node.position);
            writer.Write(node.rotation);
            writer.Write(node.scale);
            writer.Write(node.mesh);
        }
        writer.WriteVarUInt(m_materials.size());
        for (const EmbeddedMaterial& material : m_materials)
        {
            writer.WriteString(material.name);
            writer.Write(material.baseColor);
            writer.Write(material.metallic);
            writer.Write(material.roughness);
            writer.WriteString(material.baseColorTexture);
            writer.WriteString(material.normalTexture);
        }
        return std::move(writer.GetBuffer());
    }

    bool ModelAsset::Deserialize(const uint8_t* data, size_t size)
    {
        Serialization::BinaryReader reader(data, size);
        uint32_t version = 0;
        if (!reader.Read(version) || version != ModelFormatVersion)
        {
            return false;
        }
        uint64_t meshCount = 0;
        reader.ReadVarUInt(meshCount);
        m_meshes.resize(static_cast<size_t>(meshCount));
        for (MeshData& mesh : m_meshes)
        {
            reader.ReadString(mesh.name);
            reader.ReadVector(mesh.vertices);
            reader.ReadVector(mesh.indices);
            reader.ReadVector(mesh.subMeshes);
            reader.Read(mesh.bounds);
        }
        uint64_t nodeCount = 0;
        reader.ReadVarUInt(nodeCount);
        m_nodes.resize(static_cast<size_t>(nodeCount));
        for (ModelNode& node : m_nodes)
        {
            reader.ReadString(node.name);
            reader.Read(node.parent);
            reader.Read(node.position);
            reader.Read(node.rotation);
            reader.Read(node.scale);
            reader.Read(node.mesh);
        }
        uint64_t materialCount = 0;
        reader.ReadVarUInt(materialCount);
        m_materials.resize(static_cast<size_t>(materialCount));
        for (EmbeddedMaterial& material : m_materials)
        {
            reader.ReadString(material.name);
            reader.Read(material.baseColor);
            reader.Read(material.metallic);
            reader.Read(material.roughness);
            reader.ReadString(material.baseColorTexture);
            reader.ReadString(material.normalTexture);
        }
        return !reader.Failed();
    }
}
