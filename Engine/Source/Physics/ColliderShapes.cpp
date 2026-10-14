#include "Physics/ColliderShapes.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Geometry/ModelAsset.h"
#include "Level/LevelObject.h"
#include "Physics/Colliders.h"
#include "Physics/JoltConversions.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSettings.h>

#include <algorithm>

namespace BrightBox3D::Physics
{
    namespace
    {
        constexpr float MinimumShapeExtent = 0.001f;
        constexpr float UnitScaleTolerance = 1.0e-6f;

        JPH::RefConst<JPH::Shape> CreateBoxShape(const BoxCollider& box)
        {
            JPH::Vec3 half = ToJolt(box.GetSize() * 0.5f).Abs();
            half = JPH::Vec3::sMax(half, JPH::Vec3::sReplicate(MinimumShapeExtent));
            float radius = std::min(JPH::cDefaultConvexRadius, half.ReduceMin() * 0.5f);
            return new JPH::BoxShape(half, radius);
        }

        JPH::RefConst<JPH::Shape> CreateSphereShape(const SphereCollider& sphere)
        {
            return new JPH::SphereShape(std::max(sphere.GetRadius(), MinimumShapeExtent));
        }

        JPH::RefConst<JPH::Shape> CreateCapsuleShape(const CapsuleCollider& capsule)
        {
            float radius = std::max(capsule.GetRadius(), MinimumShapeExtent);
            float halfHeight = std::max(capsule.GetHeight() * 0.5f - radius, MinimumShapeExtent);
            return new JPH::CapsuleShape(halfHeight, radius);
        }

        JPH::RefConst<JPH::Shape> CreateConvexHullShape(const Geometry::MeshData& data)
        {
            JPH::Array<JPH::Vec3> points;
            points.reserve(data.vertices.size());
            for (const Geometry::MeshVertex& vertex : data.vertices)
            {
                points.push_back(ToJolt(vertex.position));
            }
            JPH::ConvexHullShapeSettings settings(points);
            JPH::ShapeSettings::ShapeResult result = settings.Create();
            if (result.HasError())
            {
                Core::Log::Error(Core::Text(Core::EngineText::ConvexHullFailed, result.GetError().c_str()));
                return nullptr;
            }
            return result.Get();
        }

        JPH::RefConst<JPH::Shape> CreateTriangleMeshShape(const Geometry::MeshData& data)
        {
            JPH::VertexList vertices;
            vertices.reserve(data.vertices.size());
            for (const Geometry::MeshVertex& vertex : data.vertices)
            {
                vertices.push_back(ToJoltFloat3(vertex.position));
            }
            JPH::IndexedTriangleList triangles;
            triangles.reserve(data.indices.size() / 3);
            for (size_t i = 0; i + 2 < data.indices.size(); i += 3)
            {
                triangles.push_back(JPH::IndexedTriangle(data.indices[i], data.indices[i + 1], data.indices[i + 2]));
            }
            JPH::MeshShapeSettings settings(vertices, triangles);
            JPH::ShapeSettings::ShapeResult result = settings.Create();
            if (result.HasError())
            {
                Core::Log::Error(Core::Text(Core::EngineText::MeshColliderFailed, result.GetError().c_str()));
                return nullptr;
            }
            return result.Get();
        }

        JPH::RefConst<JPH::Shape> CreateMeshShape(const MeshCollider& mesh, bool dynamic)
        {
            auto model = mesh.GetModel().Load();
            const Geometry::MeshData* data = model ? model->GetMesh(mesh.GetMeshIndex()) : nullptr;
            if (!data || data->vertices.empty())
            {
                return nullptr;
            }
            if (!mesh.IsConvex() && dynamic)
            {
                Core::Log::Warning(Core::Text(Core::EngineText::ConvexHullFallback, mesh.GetOwner()->GetName()));
            }
            return mesh.IsConvex() || dynamic ? CreateConvexHullShape(*data) : CreateTriangleMeshShape(*data);
        }

        JPH::RefConst<JPH::Shape> CreateLocalShape(const Collider& collider, bool dynamic)
        {
            if (auto* box = Reflection::Cast<BoxCollider>(&collider))
            {
                return CreateBoxShape(*box);
            }
            if (auto* sphere = Reflection::Cast<SphereCollider>(&collider))
            {
                return CreateSphereShape(*sphere);
            }
            if (auto* capsule = Reflection::Cast<CapsuleCollider>(&collider))
            {
                return CreateCapsuleShape(*capsule);
            }
            if (auto* mesh = Reflection::Cast<MeshCollider>(&collider))
            {
                return CreateMeshShape(*mesh, dynamic);
            }
            return nullptr;
        }
    }

    JPH::RefConst<JPH::Shape> CreateColliderShape(const Collider& collider, bool dynamic)
    {
        JPH::RefConst<JPH::Shape> shape = CreateLocalShape(collider, dynamic);
        if (shape && collider.GetCenter().LengthSquared() > 0.0f)
        {
            shape = new JPH::RotatedTranslatedShape(ToJolt(collider.GetCenter()), JPH::Quat::sIdentity(), shape);
        }
        return shape;
    }

    JPH::RefConst<JPH::Shape> CombineShapes(const std::vector<JPH::RefConst<JPH::Shape>>& shapes)
    {
        if (shapes.empty())
        {
            return nullptr;
        }
        if (shapes.size() == 1)
        {
            return shapes.front();
        }
        JPH::StaticCompoundShapeSettings compound;
        for (const auto& shape : shapes)
        {
            compound.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), shape);
        }
        JPH::ShapeSettings::ShapeResult result = compound.Create();
        return result.HasError() ? nullptr : result.Get();
    }

    JPH::RefConst<JPH::Shape> ApplyScale(const JPH::RefConst<JPH::Shape>& shape, const Math::Vector3& scale)
    {
        if (!shape)
        {
            return nullptr;
        }
        JPH::Vec3 scaleVector = ToJolt(scale);
        if (scaleVector.IsClose(JPH::Vec3::sReplicate(1.0f), UnitScaleTolerance))
        {
            return shape;
        }
        JPH::Vec3 valid = shape->MakeScaleValid(scaleVector);
        return new JPH::ScaledShape(shape, valid);
    }
}
