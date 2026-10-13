#pragma once

#include "Assets/AssetDatabase.h"
#include "Geometry/Categories.h"
#include "Geometry/ModelAsset.h"
#include "Level/LevelObjectComponent.h"
#include "Math/Vector3.h"
#include "Physics/Categories.h"

namespace BrightBox3D::Physics
{
    BrightClass(.icon = Icon::SquareDashed, .abstract = true)
    class Collider : public Level::LevelObjectComponent
    {
        BrightBody()
    public:
        void OnPropertiesChanged() override;
        bool IsTrigger() const { return m_isTrigger; }
        void SetTrigger(bool trigger) { m_isTrigger = trigger; }
        const Math::Vector3& GetCenter() const { return m_center; }
        void SetCenter(const Math::Vector3& center) { m_center = center; }
        float GetFriction() const { return m_friction; }
        float GetRestitution() const { return m_restitution; }

    protected:
        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        bool m_isTrigger = false;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        Math::Vector3 m_center;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Material)
        float m_friction = 0.5f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Material)
        float m_restitution = 0.0f;
    };

    BrightClass(.category = Categories::Physics)
    class BoxCollider : public Collider
    {
        BrightBody()
    public:
        const Math::Vector3& GetSize() const { return m_size; }
        void SetSize(const Math::Vector3& size) { m_size = size; }

    private:
        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        Math::Vector3 m_size{ 1.0f, 1.0f, 1.0f };
    };

    BrightClass(.category = Categories::Physics)
    class SphereCollider : public Collider
    {
        BrightBody()
    public:
        float GetRadius() const { return m_radius; }
        void SetRadius(float radius) { m_radius = radius; }

    private:
        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        float m_radius = 0.5f;
    };

    BrightClass(.category = Categories::Physics)
    class CapsuleCollider : public Collider
    {
        BrightBody()
    public:
        float GetRadius() const { return m_radius; }
        float GetHeight() const { return m_height; }
        void SetSize(float radius, float height)
        {
            m_radius = radius;
            m_height = height;
        }

    private:
        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        float m_radius = 0.5f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        float m_height = 2.0f;
    };

    BrightClass(.category = Categories::Physics)
    class MeshCollider : public Collider
    {
        BrightBody()
    public:
        bool IsConvex() const { return m_convex; }
        void SetConvex(bool convex) { m_convex = convex; }
        const Assets::AssetReference<Geometry::ModelAsset>& GetModel() const { return m_model; }
        int32_t GetMeshIndex() const { return m_meshIndex; }
        void SetMesh(const Assets::AssetReference<Geometry::ModelAsset>& model, int32_t meshIndex)
        {
            m_model = model;
            m_meshIndex = meshIndex;
        }

    private:
        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Collider)
        bool m_convex = false;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Mesh)
        Assets::AssetReference<Geometry::ModelAsset> m_model;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Mesh)
        int32_t m_meshIndex = 0;
    };
}
