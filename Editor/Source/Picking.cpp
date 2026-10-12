#include "Picking.h"

#include "Assets/AssetDatabase.h"
#include "Level/Level.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/ModelAsset.h"

#include <limits>

namespace BrightBox3D::Editor
{
    PickResult PickObject(const Level::Level& level, const Math::Ray& ray)
    {
        PickResult result;
        result.distance = std::numeric_limits<float>::max();
        for (Rendering::MeshRenderer* renderer : level.FindComponentsOfType<Rendering::MeshRenderer>())
        {
            auto model = renderer->GetModel().Load();
            const Rendering::MeshData* mesh = model ? model->GetMesh(renderer->GetMeshIndex()) : nullptr;
            if (!mesh)
            {
                continue;
            }
            const Math::Matrix4& world = renderer->GetTransform().GetWorldMatrix();
            float boundsDistance = 0.0f;
            if (!ray.IntersectsBounds(mesh->bounds.Transformed(world), boundsDistance) || boundsDistance > result.distance)
            {
                continue;
            }
            Math::Matrix4 inverse = world.Inverse();
            Math::Ray local{ inverse.TransformPoint(ray.origin), inverse.TransformDirection(ray.direction) };
            float scale = local.direction.Length();
            if (scale < Math::Epsilon)
            {
                continue;
            }
            local.direction = local.direction / scale;
            for (size_t i = 0; i + 2 < mesh->indices.size(); i += 3)
            {
                float hit = 0.0f;
                const Math::Vector3& a = mesh->vertices[mesh->indices[i]].position;
                const Math::Vector3& b = mesh->vertices[mesh->indices[i + 1]].position;
                const Math::Vector3& c = mesh->vertices[mesh->indices[i + 2]].position;
                if (local.IntersectsTriangle(a, b, c, hit))
                {
                    Math::Vector3 worldPoint = world.TransformPoint(local.GetPoint(hit));
                    float distance = (worldPoint - ray.origin).Length();
                    if (distance < result.distance)
                    {
                        result.distance = distance;
                        result.object = renderer->GetOwner();
                        result.point = worldPoint;
                    }
                }
            }
        }
        return result;
    }
}
