#include "Rendering/BuiltinAssets.h"

#include "Assets/AssetDatabase.h"
#include "Geometry/ModelAsset.h"
#include "Rendering/MaterialAsset.h"

#include <iterator>
#include <string>

namespace BrightBox3D::Rendering
{
    namespace
    {
        void AddTriangle(Geometry::MeshData& mesh, uint32_t a, uint32_t b, uint32_t c, const Math::Vector3& outward)
        {
            const Math::Vector3& pa = mesh.vertices[a].position;
            const Math::Vector3& pb = mesh.vertices[b].position;
            const Math::Vector3& pc = mesh.vertices[c].position;
            if (Math::Vector3::Dot(Math::Vector3::Cross(pb - pa, pc - pa), outward) < 0.0f)
            {
                std::swap(b, c);
            }
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
        }

        void AddFace(Geometry::MeshData& mesh, const Math::Vector3& center, const Math::Vector3& normal, const Math::Vector3& up, float halfWidth, float halfHeight)
        {
            Math::Vector3 right = Math::Vector3::Cross(normal, up);
            uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
            Math::Vector3 corners[4] = {
                center - right * halfWidth + up * halfHeight,
                center + right * halfWidth + up * halfHeight,
                center + right * halfWidth - up * halfHeight,
                center - right * halfWidth - up * halfHeight
            };
            Math::Vector2 uvs[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
            for (int i = 0; i < 4; ++i)
            {
                Geometry::MeshVertex vertex;
                vertex.position = corners[i];
                vertex.normal = normal;
                vertex.tangent = Math::Vector4(right, 1.0f);
                vertex.uv = uvs[i];
                mesh.vertices.push_back(vertex);
            }
            AddTriangle(mesh, base, base + 1, base + 2, normal);
            AddTriangle(mesh, base, base + 2, base + 3, normal);
        }

        void Finish(Geometry::MeshData& mesh, const std::string& name)
        {
            mesh.name = name;
            mesh.subMeshes = { { 0, static_cast<uint32_t>(mesh.indices.size()), 0 } };
            mesh.ComputeTangents();
            mesh.ComputeBounds();
        }

        std::shared_ptr<Assets::Asset> MakeModel(Geometry::MeshData mesh)
        {
            auto model = std::make_shared<Geometry::ModelAsset>();
            Geometry::ModelNode node;
            node.name = mesh.name;
            node.mesh = 0;
            model->GetNodes().push_back(node);
            Geometry::EmbeddedMaterial material;
            material.name = BuiltinAssets::DefaultMaterialName;
            model->GetMaterials().push_back(material);
            model->GetMeshes().push_back(std::move(mesh));
            return model;
        }

        std::string BuiltinPath(const char* name)
        {
            return std::string(Assets::AssetDatabase::BuiltinFolder) + "/" + name;
        }

        struct PrimitiveInfo
        {
            const char* name;
            Core::Guid (*guid)();
            Geometry::MeshData (*create)();
        };

        const PrimitiveInfo Primitives[] = {
            { "Cube", &BuiltinAssets::Cube, [] { return BuiltinAssets::CreateCube(); } },
            { "Sphere", &BuiltinAssets::Sphere, [] { return BuiltinAssets::CreateSphere(); } },
            { "Plane", &BuiltinAssets::Plane, [] { return BuiltinAssets::CreatePlane(); } },
            { "Quad", &BuiltinAssets::Quad, [] { return BuiltinAssets::CreateQuad(); } },
            { "Cylinder", &BuiltinAssets::Cylinder, [] { return BuiltinAssets::CreateCylinder(); } },
            { "Capsule", &BuiltinAssets::Capsule, [] { return BuiltinAssets::CreateCapsule(); } },
        };

        static_assert(std::size(Primitives) == static_cast<size_t>(BuiltinPrimitive::Count));
    }

    Geometry::MeshData BuiltinAssets::CreateCube()
    {
        Geometry::MeshData mesh;
        AddFace(mesh, { 0, 0, -0.5f }, { 0, 0, -1 }, { 0, 1, 0 }, 0.5f, 0.5f);
        AddFace(mesh, { 0, 0, 0.5f }, { 0, 0, 1 }, { 0, 1, 0 }, 0.5f, 0.5f);
        AddFace(mesh, { -0.5f, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, 0.5f, 0.5f);
        AddFace(mesh, { 0.5f, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, 0.5f, 0.5f);
        AddFace(mesh, { 0, 0.5f, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, 0.5f, 0.5f);
        AddFace(mesh, { 0, -0.5f, 0 }, { 0, -1, 0 }, { 0, 0, -1 }, 0.5f, 0.5f);
        Finish(mesh, PrimitiveName(BuiltinPrimitive::Cube));
        return mesh;
    }

    Geometry::MeshData BuiltinAssets::CreateSphere(int segments, int rings)
    {
        Geometry::MeshData mesh;
        for (int ring = 0; ring <= rings; ++ring)
        {
            float v = static_cast<float>(ring) / static_cast<float>(rings);
            float phi = v * Math::Pi;
            for (int segment = 0; segment <= segments; ++segment)
            {
                float u = static_cast<float>(segment) / static_cast<float>(segments);
                float theta = u * Math::TwoPi;
                Math::Vector3 normal(std::sin(phi) * std::sin(theta), std::cos(phi), std::sin(phi) * std::cos(theta));
                Geometry::MeshVertex vertex;
                vertex.position = normal * 0.5f;
                vertex.normal = normal;
                vertex.uv = { u, v };
                mesh.vertices.push_back(vertex);
            }
        }
        for (int ring = 0; ring < rings; ++ring)
        {
            for (int segment = 0; segment < segments; ++segment)
            {
                uint32_t a = static_cast<uint32_t>(ring * (segments + 1) + segment);
                uint32_t b = a + static_cast<uint32_t>(segments + 1);
                Math::Vector3 center = (mesh.vertices[a].position + mesh.vertices[b + 1].position) * 0.5f;
                if (ring != 0)
                {
                    AddTriangle(mesh, a, a + 1, b, center);
                }
                if (ring != rings - 1)
                {
                    AddTriangle(mesh, a + 1, b + 1, b, center);
                }
            }
        }
        Finish(mesh, PrimitiveName(BuiltinPrimitive::Sphere));
        return mesh;
    }

    Geometry::MeshData BuiltinAssets::CreatePlane(int subdivisions)
    {
        Geometry::MeshData mesh;
        float size = 10.0f;
        for (int z = 0; z <= subdivisions; ++z)
        {
            for (int x = 0; x <= subdivisions; ++x)
            {
                float u = static_cast<float>(x) / static_cast<float>(subdivisions);
                float v = static_cast<float>(z) / static_cast<float>(subdivisions);
                Geometry::MeshVertex vertex;
                vertex.position = { (u - 0.5f) * size, 0.0f, (0.5f - v) * size };
                vertex.normal = { 0, 1, 0 };
                vertex.uv = { u, v };
                mesh.vertices.push_back(vertex);
            }
        }
        for (int z = 0; z < subdivisions; ++z)
        {
            for (int x = 0; x < subdivisions; ++x)
            {
                uint32_t a = static_cast<uint32_t>(z * (subdivisions + 1) + x);
                uint32_t b = a + static_cast<uint32_t>(subdivisions + 1);
                AddTriangle(mesh, a, a + 1, b + 1, { 0, 1, 0 });
                AddTriangle(mesh, a, b + 1, b, { 0, 1, 0 });
            }
        }
        Finish(mesh, PrimitiveName(BuiltinPrimitive::Plane));
        return mesh;
    }

    Geometry::MeshData BuiltinAssets::CreateQuad()
    {
        Geometry::MeshData mesh;
        AddFace(mesh, { 0, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 }, 0.5f, 0.5f);
        Finish(mesh, PrimitiveName(BuiltinPrimitive::Quad));
        return mesh;
    }

    Geometry::MeshData BuiltinAssets::CreateCylinder(int segments)
    {
        Geometry::MeshData mesh;
        float radius = 0.5f;
        float half = 1.0f;
        for (int side = 0; side <= segments; ++side)
        {
            float u = static_cast<float>(side) / static_cast<float>(segments);
            float theta = u * Math::TwoPi;
            Math::Vector3 normal(std::sin(theta), 0.0f, std::cos(theta));
            for (int level = 0; level < 2; ++level)
            {
                Geometry::MeshVertex vertex;
                vertex.position = normal * radius + Math::Vector3(0.0f, level == 0 ? half : -half, 0.0f);
                vertex.normal = normal;
                vertex.uv = { u, static_cast<float>(level) };
                mesh.vertices.push_back(vertex);
            }
        }
        for (int side = 0; side < segments; ++side)
        {
            uint32_t a = static_cast<uint32_t>(side * 2);
            Math::Vector3 outward = mesh.vertices[a].normal + mesh.vertices[a + 2].normal;
            AddTriangle(mesh, a, a + 2, a + 1, outward);
            AddTriangle(mesh, a + 2, a + 3, a + 1, outward);
        }
        for (int cap = 0; cap < 2; ++cap)
        {
            float y = cap == 0 ? half : -half;
            Math::Vector3 normal(0.0f, cap == 0 ? 1.0f : -1.0f, 0.0f);
            uint32_t center = static_cast<uint32_t>(mesh.vertices.size());
            Geometry::MeshVertex middle;
            middle.position = { 0.0f, y, 0.0f };
            middle.normal = normal;
            middle.uv = { 0.5f, 0.5f };
            mesh.vertices.push_back(middle);
            for (int side = 0; side <= segments; ++side)
            {
                float theta = static_cast<float>(side) / static_cast<float>(segments) * Math::TwoPi;
                Geometry::MeshVertex vertex;
                vertex.position = { std::sin(theta) * radius, y, std::cos(theta) * radius };
                vertex.normal = normal;
                vertex.uv = { 0.5f + std::sin(theta) * 0.5f, 0.5f + std::cos(theta) * 0.5f };
                mesh.vertices.push_back(vertex);
            }
            for (int side = 0; side < segments; ++side)
            {
                AddTriangle(mesh, center, center + 1 + static_cast<uint32_t>(side), center + 2 + static_cast<uint32_t>(side), normal);
            }
        }
        Finish(mesh, PrimitiveName(BuiltinPrimitive::Cylinder));
        return mesh;
    }

    Geometry::MeshData BuiltinAssets::CreateCapsule(int segments, int rings)
    {
        Geometry::MeshData mesh;
        float radius = 0.5f;
        float halfCylinder = 0.5f;
        int totalRings = rings * 2 + 1;
        for (int ring = 0; ring <= totalRings; ++ring)
        {
            float phi;
            float offset;
            if (ring <= rings)
            {
                phi = static_cast<float>(ring) / static_cast<float>(rings) * Math::HalfPi;
                offset = halfCylinder;
            }
            else
            {
                phi = Math::HalfPi + static_cast<float>(ring - rings - 1) / static_cast<float>(rings) * Math::HalfPi;
                offset = -halfCylinder;
            }
            for (int segment = 0; segment <= segments; ++segment)
            {
                float u = static_cast<float>(segment) / static_cast<float>(segments);
                float theta = u * Math::TwoPi;
                Math::Vector3 normal(std::sin(phi) * std::sin(theta), std::cos(phi), std::sin(phi) * std::cos(theta));
                Geometry::MeshVertex vertex;
                vertex.position = normal * radius + Math::Vector3(0.0f, offset, 0.0f);
                vertex.normal = normal;
                vertex.uv = { u, static_cast<float>(ring) / static_cast<float>(totalRings) };
                mesh.vertices.push_back(vertex);
            }
        }
        for (int ring = 0; ring < totalRings; ++ring)
        {
            for (int segment = 0; segment < segments; ++segment)
            {
                uint32_t a = static_cast<uint32_t>(ring * (segments + 1) + segment);
                uint32_t b = a + static_cast<uint32_t>(segments + 1);
                Math::Vector3 outward = mesh.vertices[a].normal + mesh.vertices[b + 1].normal + mesh.vertices[a + 1].normal + mesh.vertices[b].normal;
                if (ring != 0)
                {
                    AddTriangle(mesh, a, a + 1, b, outward);
                }
                if (ring != totalRings - 1)
                {
                    AddTriangle(mesh, a + 1, b + 1, b, outward);
                }
            }
        }
        Finish(mesh, PrimitiveName(BuiltinPrimitive::Capsule));
        return mesh;
    }

    const char* BuiltinAssets::PrimitiveName(BuiltinPrimitive primitive)
    {
        return Primitives[static_cast<size_t>(primitive)].name;
    }

    Core::Guid BuiltinAssets::PrimitiveGuid(BuiltinPrimitive primitive)
    {
        return Primitives[static_cast<size_t>(primitive)].guid();
    }

    void BuiltinAssets::Register()
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        for (const PrimitiveInfo& primitive : Primitives)
        {
            database.RegisterBuiltin(primitive.guid(), BuiltinPath(primitive.name), Assets::AssetType::Model, [create = primitive.create] { return MakeModel(create()); });
        }
        database.RegisterBuiltin(DefaultMaterial(), BuiltinPath(DefaultMaterialName), Assets::AssetType::Material, []
        {
            auto material = std::make_shared<MaterialAsset>();
            material->m_baseColor = { 0.8f, 0.8f, 0.8f, 1.0f };
            return material;
        });
    }
}
