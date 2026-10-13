#pragma once

#include "Math/Geometry.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"

#include <cstdint>
#include <string>
#include <vector>

namespace BrightBox3D::Geometry
{
    struct MeshVertex
    {
        Math::Vector3 position;
        Math::Vector3 normal{ 0.0f, 1.0f, 0.0f };
        Math::Vector4 tangent{ 1.0f, 0.0f, 0.0f, 1.0f };
        Math::Vector2 uv;
    };

    struct SubMesh
    {
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        uint32_t materialSlot = 0;
    };

    struct MeshData
    {
        std::string name;
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<SubMesh> subMeshes;
        Math::Bounds bounds;

        void ComputeBounds();
        void ComputeTangents();
        void ComputeNormals();
    };

    struct EmbeddedMaterial
    {
        std::string name;
        Math::Vector4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float metallic = 0.0f;
        float roughness = 0.5f;
        std::string baseColorTexture;
        std::string normalTexture;
    };

    struct ModelNode
    {
        std::string name;
        int32_t parent = -1;
        Math::Vector3 position;
        Math::Vector4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
        Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };
        int32_t mesh = -1;
    };
}
