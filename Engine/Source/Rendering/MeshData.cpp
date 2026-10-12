#include "Rendering/MeshData.h"

namespace BrightBox3D::Rendering
{
    void MeshData::ComputeBounds()
    {
        bounds = Math::Bounds();
        for (const MeshVertex& vertex : vertices)
        {
            bounds.Encapsulate(vertex.position);
        }
    }

    void MeshData::ComputeNormals()
    {
        std::vector<Math::Vector3> normals(vertices.size());
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const Math::Vector3& a = vertices[indices[i]].position;
            const Math::Vector3& b = vertices[indices[i + 1]].position;
            const Math::Vector3& c = vertices[indices[i + 2]].position;
            Math::Vector3 normal = Math::Vector3::Cross(b - a, c - a);
            normals[indices[i]] += normal;
            normals[indices[i + 1]] += normal;
            normals[indices[i + 2]] += normal;
        }
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            vertices[i].normal = normals[i].Normalized();
        }
    }

    void MeshData::ComputeTangents()
    {
        std::vector<Math::Vector3> tangents(vertices.size());
        std::vector<Math::Vector3> bitangents(vertices.size());
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            const MeshVertex& v0 = vertices[i0];
            const MeshVertex& v1 = vertices[i1];
            const MeshVertex& v2 = vertices[i2];
            Math::Vector3 edge1 = v1.position - v0.position;
            Math::Vector3 edge2 = v2.position - v0.position;
            Math::Vector2 uv1 = v1.uv - v0.uv;
            Math::Vector2 uv2 = v2.uv - v0.uv;
            float determinant = uv1.x * uv2.y - uv2.x * uv1.y;
            if (std::abs(determinant) < 1.0e-12f)
            {
                continue;
            }
            float inverse = 1.0f / determinant;
            Math::Vector3 tangent = (edge1 * uv2.y - edge2 * uv1.y) * inverse;
            Math::Vector3 bitangent = (edge2 * uv1.x - edge1 * uv2.x) * inverse;
            for (uint32_t index : { i0, i1, i2 })
            {
                tangents[index] += tangent;
                bitangents[index] += bitangent;
            }
        }
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            Math::Vector3 normal = vertices[i].normal;
            Math::Vector3 tangent = tangents[i] - normal * Math::Vector3::Dot(normal, tangents[i]);
            if (tangent.LengthSquared() < 1.0e-12f)
            {
                Math::Vector3 axis = std::abs(normal.y) < 0.99f ? Math::Vector3::Up() : Math::Vector3::Right();
                tangent = Math::Vector3::Cross(axis, normal);
            }
            tangent = tangent.Normalized();
            float handedness = Math::Vector3::Dot(Math::Vector3::Cross(normal, tangent), bitangents[i]) < 0.0f ? -1.0f : 1.0f;
            vertices[i].tangent = Math::Vector4(tangent, handedness);
        }
    }
}
