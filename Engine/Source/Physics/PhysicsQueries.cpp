#include "Physics/PhysicsWorldImplementation.h"

#include "Level/Level.h"
#include "Physics/JoltConversions.h"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>

namespace BrightBox3D::Physics
{
    namespace
    {
        constexpr float MinimumKinematicDeltaTime = 1.0e-4f;

        class IgnoreSensors final : public JPH::BodyFilter
        {
        public:
            bool ShouldCollideLocked(const JPH::Body& body) const override { return !body.IsSensor(); }
        };

        JPH::BodyID BodyOf(const Rigidbody& body)
        {
            return JPH::BodyID(body.GetBodyId());
        }
    }

    JPH::BodyInterface* PhysicsWorld::Implementation::BodiesFor(const Rigidbody& rigidbody) const
    {
        if (!system || BodyOf(rigidbody).IsInvalid())
        {
            return nullptr;
        }
        return &system->GetBodyInterface();
    }

    bool PhysicsWorld::Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& hit, TriggerQuery triggers) const
    {
        Implementation& implementation = *m_implementation;
        if (!implementation.system)
        {
            return false;
        }
        Math::Vector3 normalized = direction.Normalized();
        JPH::RRayCast ray(ToJoltPosition(origin), ToJolt(normalized * maxDistance));
        JPH::RayCastResult result;
        IgnoreSensors ignoreSensors;
        JPH::BodyFilter allBodies;
        bool found = implementation.system->GetNarrowPhaseQuery().CastRay(ray, result, {}, {}, triggers == TriggerQuery::Include ? allBodies : static_cast<const JPH::BodyFilter&>(ignoreSensors));
        if (!found)
        {
            return false;
        }
        auto owner = implementation.owners.find(result.mBodyID.GetIndexAndSequenceNumber());
        hit.object = owner == implementation.owners.end() ? nullptr : GetLevel()->FindObject(owner->second);
        hit.distance = result.mFraction * maxDistance;
        hit.point = origin + normalized * hit.distance;
        JPH::BodyLockRead lock(implementation.system->GetBodyLockInterface(), result.mBodyID);
        if (lock.Succeeded())
        {
            hit.normal = FromJolt(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ray.GetPointOnRay(result.mFraction)));
        }
        return true;
    }

    bool PhysicsWorld::Raycast(const Level::Level* level, const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& hit)
    {
        PhysicsWorld* world = Get(level);
        return world && world->Raycast(origin, direction, maxDistance, hit);
    }

    Math::Vector3 PhysicsWorld::GetLinearVelocity(const Rigidbody& body) const
    {
        JPH::BodyInterface* bodies = m_implementation->BodiesFor(body);
        return bodies ? FromJolt(bodies->GetLinearVelocity(BodyOf(body))) : Math::Vector3();
    }

    void PhysicsWorld::SetLinearVelocity(Rigidbody& body, const Math::Vector3& velocity)
    {
        if (JPH::BodyInterface* bodies = m_implementation->BodiesFor(body))
        {
            bodies->SetLinearVelocity(BodyOf(body), ToJolt(velocity));
        }
    }

    Math::Vector3 PhysicsWorld::GetAngularVelocity(const Rigidbody& body) const
    {
        JPH::BodyInterface* bodies = m_implementation->BodiesFor(body);
        return bodies ? FromJolt(bodies->GetAngularVelocity(BodyOf(body))) : Math::Vector3();
    }

    void PhysicsWorld::SetAngularVelocity(Rigidbody& body, const Math::Vector3& velocity)
    {
        if (JPH::BodyInterface* bodies = m_implementation->BodiesFor(body))
        {
            bodies->SetAngularVelocity(BodyOf(body), ToJolt(velocity));
        }
    }

    void PhysicsWorld::AddForce(Rigidbody& body, const Math::Vector3& force)
    {
        if (JPH::BodyInterface* bodies = m_implementation->BodiesFor(body))
        {
            bodies->AddForce(BodyOf(body), ToJolt(force));
        }
    }

    void PhysicsWorld::AddImpulse(Rigidbody& body, const Math::Vector3& impulse)
    {
        if (JPH::BodyInterface* bodies = m_implementation->BodiesFor(body))
        {
            bodies->AddImpulse(BodyOf(body), ToJolt(impulse));
        }
    }

    void PhysicsWorld::AddTorque(Rigidbody& body, const Math::Vector3& torque)
    {
        if (JPH::BodyInterface* bodies = m_implementation->BodiesFor(body))
        {
            bodies->AddTorque(BodyOf(body), ToJolt(torque));
        }
    }

    void PhysicsWorld::MoveKinematic(Rigidbody& body, const Math::Vector3& position, float deltaTime)
    {
        if (JPH::BodyInterface* bodies = m_implementation->BodiesFor(body))
        {
            bodies->MoveKinematic(BodyOf(body), ToJoltPosition(position), bodies->GetRotation(BodyOf(body)), std::max(deltaTime, MinimumKinematicDeltaTime));
        }
    }

    bool PhysicsWorld::IsSleeping(const Rigidbody& body) const
    {
        JPH::BodyInterface* bodies = m_implementation->BodiesFor(body);
        return bodies && !bodies->IsActive(BodyOf(body));
    }

    void PhysicsWorld::WakeUp(Rigidbody& body)
    {
        if (JPH::BodyInterface* bodies = m_implementation->BodiesFor(body))
        {
            bodies->ActivateBody(BodyOf(body));
        }
    }

    void PhysicsWorld::SetGravityScale(Rigidbody& body, float scale)
    {
        if (JPH::BodyInterface* bodies = m_implementation->BodiesFor(body))
        {
            bodies->SetGravityFactor(BodyOf(body), scale);
        }
    }
}
