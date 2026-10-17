#pragma once

#include "Assets/AssetDatabase.h"
#include "Geometry/Categories.h"
#include "Geometry/ModelAsset.h"
#include "Level/LevelObjectComponent.h"
#include "Math/Vector3.h"
#include "Physics/Categories.h"

#include <cstdint>

namespace BrightBox3D::Physics
{
    BrightEnum()
    enum class ColliderShape
    {
        Box,
        Sphere,
        Mesh
    };

    struct ColliderMesh
    {
        Assets::AssetReference<Geometry::ModelAsset> model;
        int32_t meshIndex = 0;
    };

    BrightClass(.category = Categories::Physics, .icon = Icon::SquareDashed)
    class Collider : public Level::LevelObjectComponent
    {
        BrightBody()
    public:
        void OnPropertiesChanged() override;
        void OnPropertyEdited(const Reflection::PropertyInfo& property) override;
        bool IsPropertyVisible(const Reflection::PropertyInfo& property) const override;

        ColliderShape GetShape() const { return m_shape; }
        void SetShape(ColliderShape shape);
        bool IsTrigger() const { return m_isTrigger; }
        void SetTrigger(bool trigger) { m_isTrigger = trigger; }
        const Math::Vector3& GetCenter() const { return m_center; }
        void SetCenter(const Math::Vector3& center) { m_center = center; }
        const Math::Vector3& GetSize() const { return m_size; }
        void SetSize(const Math::Vector3& size) { m_size = size; }
        float GetDiameter() const { return m_diameter; }
        void SetDiameter(float diameter) { m_diameter = diameter; }
        bool IsConvex() const { return m_convex; }
        void SetConvex(bool convex) { m_convex = convex; }
        const Assets::AssetReference<Geometry::ModelAsset>& GetModel() const { return m_model; }
        int32_t GetMeshIndex() const { return m_meshIndex; }
        void SetMesh(const Assets::AssetReference<Geometry::ModelAsset>& model, int32_t meshIndex);
        ColliderMesh ResolveMesh() const;
        float GetFriction() const { return m_friction; }
        float GetRestitution() const { return m_restitution; }

    private:
        void ConvertSize(ColliderShape from, ColliderShape to);

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        ColliderShape m_shape = ColliderShape::Box;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        bool m_isTrigger = false;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        Math::Vector3 m_center;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        Math::Vector3 m_size{ 1.0f, 1.0f, 1.0f };

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        float m_diameter = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Mesh)
        Assets::AssetReference<Geometry::ModelAsset> m_model;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Mesh)
        int32_t m_meshIndex = 0;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Mesh)
        bool m_convex = false;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Material)
        float m_friction = 0.5f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Material)
        float m_restitution = 0.0f;

        ColliderShape m_appliedShape = ColliderShape::Box;
    };
}
