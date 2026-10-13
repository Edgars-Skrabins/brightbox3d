#pragma once

#include "Level/Component.h"
#include "Math/Vector3.h"

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

    BrightClass(Category = "Physics")
    class Rigidbody : public Level::Component
    {
        BrightBody()
    public:
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
        BrightProperty(EditAnywhere, Category = "Body")
        MotionType m_motionType = MotionType::Dynamic;

        BrightProperty(EditAnywhere, Category = "Body")
        float m_mass = 1.0f;

        BrightProperty(EditAnywhere, Category = "Body")
        float m_gravityScale = 1.0f;

        BrightProperty(EditAnywhere, Category = "Body")
        float m_linearDamping = 0.05f;

        BrightProperty(EditAnywhere, Category = "Body")
        float m_angularDamping = 0.05f;

        BrightProperty(EditAnywhere, Category = "Constraints")
        bool m_freezeRotation = false;

        BrightProperty(EditAnywhere, Category = "Body")
        bool m_continuousCollision = false;

        uint32_t m_bodyId = 0xFFFFFFFFu;
    };
}
