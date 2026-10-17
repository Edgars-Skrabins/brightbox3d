#include "Physics/Collider.h"

#include "Geometry/MeshSource.h"
#include "Level/LevelObject.h"
#include "Physics/PhysicsWorld.h"

namespace BrightBox3D::Physics
{
    namespace
    {
        const Math::Vector3 UnitSize{ 1.0f, 1.0f, 1.0f };
    }

    void Collider::OnPropertiesChanged()
    {
        m_appliedShape = m_shape;
        if (PhysicsWorld* world = PhysicsWorld::Get(GetLevel()))
        {
            world->MarkDirty(*GetOwner());
        }
    }

    void Collider::OnPropertyEdited(const Reflection::PropertyInfo& property)
    {
        if (property.GetValuePointer(this) == &m_shape && m_shape != m_appliedShape)
        {
            ConvertSize(m_appliedShape, m_shape);
        }
    }

    bool Collider::IsPropertyVisible(const Reflection::PropertyInfo& property) const
    {
        const void* value = property.GetValuePointer(this);
        if (value == &m_diameter)
        {
            return m_shape == ColliderShape::Sphere;
        }
        if (value == &m_size)
        {
            return m_shape != ColliderShape::Sphere;
        }
        if (value == &m_model || value == &m_meshIndex || value == &m_convex)
        {
            return m_shape == ColliderShape::Mesh;
        }
        return true;
    }

    void Collider::SetShape(ColliderShape shape)
    {
        if (shape != m_shape)
        {
            ConvertSize(m_shape, shape);
            m_shape = shape;
        }
        OnPropertiesChanged();
    }

    void Collider::SetMesh(const Assets::AssetReference<Geometry::ModelAsset>& model, int32_t meshIndex)
    {
        m_model = model;
        m_meshIndex = meshIndex;
    }

    ColliderMesh Collider::ResolveMesh() const
    {
        if (m_model.IsSet())
        {
            return { m_model, m_meshIndex };
        }
        for (const auto& component : GetOwner()->GetComponents())
        {
            if (const auto* source = dynamic_cast<const Geometry::MeshSource*>(component.get()))
            {
                return { source->GetModel(), source->GetMeshIndex() };
            }
        }
        return {};
    }

    void Collider::ConvertSize(ColliderShape from, ColliderShape to)
    {
        switch (to)
        {
        case ColliderShape::Sphere:
            m_diameter = m_size.x;
            break;
        case ColliderShape::Box:
            if (from == ColliderShape::Sphere)
            {
                m_size = { m_diameter, m_diameter, m_diameter };
            }
            break;
        case ColliderShape::Mesh:
            m_size = UnitSize;
            break;
        }
    }
}
