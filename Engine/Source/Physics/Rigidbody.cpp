#include "Physics/Rigidbody.h"

#include "Level/Level.h"
#include "Physics/PhysicsWorld.h"

namespace BrightBox3D::Physics
{
    void Rigidbody::OnPropertiesChanged()
    {
        if (PhysicsWorld* world = PhysicsWorld::Get(GetLevel()))
        {
            world->MarkDirty(*GetOwner());
        }
    }

    void Rigidbody::SetMotionType(MotionType type)
    {
        m_motionType = type;
        OnPropertiesChanged();
    }

    void Rigidbody::SetGravityScale(float scale)
    {
        m_gravityScale = scale;
        if (PhysicsWorld* world = PhysicsWorld::Get(GetLevel()))
        {
            world->SetGravityScale(*this, scale);
        }
    }

    Math::Vector3 Rigidbody::GetLinearVelocity() const
    {
        PhysicsWorld* world = PhysicsWorld::Get(GetLevel());
        return world ? world->GetLinearVelocity(*this) : Math::Vector3();
    }

    void Rigidbody::SetLinearVelocity(const Math::Vector3& velocity)
    {
        if (PhysicsWorld* world = PhysicsWorld::Get(GetLevel()))
        {
            world->SetLinearVelocity(*this, velocity);
        }
    }

    Math::Vector3 Rigidbody::GetAngularVelocity() const
    {
        PhysicsWorld* world = PhysicsWorld::Get(GetLevel());
        return world ? world->GetAngularVelocity(*this) : Math::Vector3();
    }

    void Rigidbody::SetAngularVelocity(const Math::Vector3& velocity)
    {
        if (PhysicsWorld* world = PhysicsWorld::Get(GetLevel()))
        {
            world->SetAngularVelocity(*this, velocity);
        }
    }

    void Rigidbody::AddForce(const Math::Vector3& force)
    {
        if (PhysicsWorld* world = PhysicsWorld::Get(GetLevel()))
        {
            world->AddForce(*this, force);
        }
    }

    void Rigidbody::AddImpulse(const Math::Vector3& impulse)
    {
        if (PhysicsWorld* world = PhysicsWorld::Get(GetLevel()))
        {
            world->AddImpulse(*this, impulse);
        }
    }

    void Rigidbody::AddTorque(const Math::Vector3& torque)
    {
        if (PhysicsWorld* world = PhysicsWorld::Get(GetLevel()))
        {
            world->AddTorque(*this, torque);
        }
    }

    void Rigidbody::MovePosition(const Math::Vector3& position)
    {
        if (PhysicsWorld* world = PhysicsWorld::Get(GetLevel()))
        {
            world->MoveKinematic(*this, position, GetLevel()->GetFixedDeltaTime());
        }
    }

    bool Rigidbody::IsSleeping() const
    {
        PhysicsWorld* world = PhysicsWorld::Get(GetLevel());
        return world && world->IsSleeping(*this);
    }

    void Rigidbody::WakeUp()
    {
        if (PhysicsWorld* world = PhysicsWorld::Get(GetLevel()))
        {
            world->WakeUp(*this);
        }
    }
}
