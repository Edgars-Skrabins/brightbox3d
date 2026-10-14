#pragma once

#include "Level/LevelObjectComponent.h"
#include "Math/Vector3.h"
#include "Physics/Categories.h"

#include <cstdint>

namespace BrightBox3D::Physics
{
    BrightEnum()
    enum class MotionType
    {
        Dynamic,
        Kinematic,
        Static
    };

    BrightClass(.category = Categories::Physics, .icon = Icon::Weight)
    class Rigidbody : public Level::LevelObjectComponent
    {
        BrightBody()
    public:
        static constexpr uint32_t InvalidBodyId = 0xFFFFFFFFu;

        void OnPropertiesChanged() override;

        MotionType GetMotionType() const { return m_motionType; }
        void SetMotionType(MotionType type);
        float GetMass() const { return m_mass; }
        void SetMass(float mass) { m_mass = mass; OnPropertiesChanged(); }
        float GetGravityScale() const { return m_gravityScale; }
        void SetGravityScale(float scale);
        float GetLinearDamping() const { return m_linearDamping; }
        float GetAngularDamping() const { return m_angularDamping; }
        bool GetFreezeRotation() const { return m_freezeRotation; }
        bool GetContinuousCollision() const { return m_continuousCollision; }

        Math::Vector3 GetLinearVelocity() const;
        void SetLinearVelocity(const Math::Vector3& velocity);
        Math::Vector3 GetAngularVelocity() const;
        void SetAngularVelocity(const Math::Vector3& velocity);
        void AddForce(const Math::Vector3& force);
        void AddImpulse(const Math::Vector3& impulse);
        void AddTorque(const Math::Vector3& torque);
        void MovePosition(const Math::Vector3& position);
        bool IsSleeping() const;
        void WakeUp();

        uint32_t GetBodyId() const { return m_bodyId; }
        void SetBodyId(uint32_t id) { m_bodyId = id; }

    private:
        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Body)
        MotionType m_motionType = MotionType::Dynamic;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Body)
        float m_mass = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Body)
        float m_gravityScale = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Body)
        float m_linearDamping = 0.05f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Body)
        float m_angularDamping = 0.05f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Constraints)
        bool m_freezeRotation = false;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Body)
        bool m_continuousCollision = false;

        uint32_t m_bodyId = InvalidBodyId;
    };
}
