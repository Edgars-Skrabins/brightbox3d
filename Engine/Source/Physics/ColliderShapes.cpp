#include "Physics/ColliderShapes.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Geometry/ModelAsset.h"
#include "Level/LevelObject.h"
#include "Physics/Collider.h"
#include "Physics/JoltConversions.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
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

        JPH::RefConst<JPH::Shape> CreateBoxShape(const Collider& box)
        {
            JPH::Vec3 half = ToJolt(box.GetSize() * 0.5f).Abs();
            half = JPH::Vec3::sMax(half, JPH::Vec3::sReplicate(MinimumShapeExtent));
            float radius = std::min(JPH::cDefaultConvexRadius, half.ReduceMin() * 0.5f);
            return new JPH::BoxShape(half, radius);
        }

        JPH::RefConst<JPH::Shape> CreateSphereShape(const Collider& sphere)
        {
            return new JPH::SphereShape(std::max(sphere.GetDiameter() * 0.5f, MinimumShapeExtent));
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

        JPH::RefConst<JPH::Shape> CreateMeshShape(const Collider& mesh, MotionType motion)
        {
            ColliderMesh source = mesh.ResolveMesh();
            auto model = source.model.Load();
            if (!model)
            {
                Assets::ReportLoadFailure(model.GetError());
            }
            const Geometry::MeshData* data = model ? model->GetMesh(source.meshIndex) : nullptr;
            if (!data || data->vertices.empty())
            {
                return nullptr;
            }
            if (!mesh.IsConvex() && motion == MotionType::Dynamic)
            {
                Core::Log::Warning(Core::Text(Core::EngineText::ConvexHullFallback, mesh.GetOwner()->GetName()));
            }
            JPH::RefConst<JPH::Shape> shape = mesh.IsConvex() || motion == MotionType::Dynamic ? CreateConvexHullShape(*data) : CreateTriangleMeshShape(*data);
            return ApplyScale(shape, mesh.GetSize());
        }

        JPH::RefConst<JPH::Shape> CreateLocalShape(const Collider& collider, MotionType motion)
        {
            switch (collider.GetShape())
            {
            case ColliderShape::Box:
                return CreateBoxShape(collider);
            case ColliderShape::Sphere:
                return CreateSphereShape(collider);
            case ColliderShape::Mesh:
                return CreateMeshShape(collider, motion);
            }
            return nullptr;
        }
    }

    JPH::RefConst<JPH::Shape> CreateColliderShape(const Collider& collider, MotionType motion)
    {
        JPH::RefConst<JPH::Shape> shape = CreateLocalShape(collider, motion);
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
