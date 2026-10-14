#include "Physics/PhysicsWorldImplementation.h"

#include "Level/Level.h"
#include "Physics/ColliderShapes.h"
#include "Physics/Colliders.h"
#include "Physics/JoltConversions.h"
#include "Physics/JoltRuntime.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <vector>

namespace BrightBox3D::Physics
{
    namespace
    {
        constexpr float MinimumMass = 0.001f;
        constexpr float ScaleChangeEpsilon = 1.0e-8f;
        constexpr float FallbackBodyRadius = 0.05f;
        constexpr float DefaultFriction = 0.5f;
        constexpr float DefaultRestitution = 0.0f;

        struct BodyShapes
        {
            std::vector<JPH::RefConst<JPH::Shape>> solids;
            std::vector<JPH::RefConst<JPH::Shape>> sensors;
            float friction = DefaultFriction;
            float restitution = DefaultRestitution;
        };

        Rigidbody* EnabledRigidbody(Level::LevelObject& object)
        {
            Rigidbody* rigidbody = object.GetComponent<Rigidbody>();
            return rigidbody && rigidbody->IsEnabled() ? rigidbody : nullptr;
        }

        JPH::EMotionType ToJoltMotionType(MotionType motion)
        {
            switch (motion)
            {
            case MotionType::Dynamic:
                return JPH::EMotionType::Dynamic;
            case MotionType::Kinematic:
                return JPH::EMotionType::Kinematic;
            case MotionType::Static:
                return JPH::EMotionType::Static;
            }
            return JPH::EMotionType::Static;
        }

        BodyShapes CollectShapes(Level::LevelObject& object, MotionType motion)
        {
            BodyShapes shapes;
            for (Collider* collider : object.GetComponents<Collider>())
            {
                if (!collider->IsEnabled())
                {
                    continue;
                }
                JPH::RefConst<JPH::Shape> shape = CreateColliderShape(*collider, motion == MotionType::Dynamic && !collider->IsTrigger());
                if (!shape)
                {
                    continue;
                }
                if (collider->IsTrigger())
                {
                    shapes.sensors.push_back(shape);
                    continue;
                }
                shapes.solids.push_back(shape);
                shapes.friction = collider->GetFriction();
                shapes.restitution = collider->GetRestitution();
            }
            if (shapes.solids.empty() && shapes.sensors.empty() && motion == MotionType::Dynamic)
            {
                shapes.solids.push_back(new JPH::SphereShape(FallbackBodyRadius));
            }
            return shapes;
        }

        void ApplyRigidbodySettings(JPH::BodyCreationSettings& settings, const Rigidbody& rigidbody)
        {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = std::max(rigidbody.GetMass(), MinimumMass);
            settings.mGravityFactor = rigidbody.GetGravityScale();
            settings.mLinearDamping = rigidbody.GetLinearDamping();
            settings.mAngularDamping = rigidbody.GetAngularDamping();
            settings.mMotionQuality = rigidbody.GetContinuousCollision() ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
            if (rigidbody.GetFreezeRotation())
            {
                settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
            }
        }

        JPH::BodyID CreateSolidBody(JPH::BodyInterface& bodies, const BodyShapes& shapes, const Level::Transform& transform, const Math::Vector3& scale, MotionType motion, const Rigidbody* rigidbody)
        {
            JPH::RefConst<JPH::Shape> shape = ApplyScale(CombineShapes(shapes.solids), scale);
            JPH::BodyCreationSettings settings(shape, ToJoltPosition(transform.GetPosition()), ToJolt(transform.GetRotation()), ToJoltMotionType(motion), motion == MotionType::Static ? StaticLayer : MovingLayer);
            settings.mFriction = shapes.friction;
            settings.mRestitution = shapes.restitution;
            if (rigidbody)
            {
                ApplyRigidbodySettings(settings, *rigidbody);
            }
            return bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
        }

        JPH::BodyID CreateSensorBody(JPH::BodyInterface& bodies, const BodyShapes& shapes, const Level::Transform& transform, const Math::Vector3& scale)
        {
            JPH::RefConst<JPH::Shape> shape = ApplyScale(CombineShapes(shapes.sensors), scale);
            JPH::BodyCreationSettings settings(shape, ToJoltPosition(transform.GetPosition()), ToJolt(transform.GetRotation()), JPH::EMotionType::Kinematic, MovingLayer);
            settings.mIsSensor = true;
            settings.mCollideKinematicVsNonDynamic = true;
            return bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
        }
    }

    void PhysicsWorld::OnPreFixedUpdate(float fixedDeltaTime)
    {
        if (!m_implementation->system)
        {
            return;
        }
        Level::Level& level = *GetLevel();
        m_implementation->DetectScaleChanges(level);
        m_implementation->RebuildDirtyBodies(level);
        m_implementation->SyncMovedBodies(level, fixedDeltaTime);
    }

    void PhysicsWorld::Implementation::DestroyBodies(const BodyRecord& record)
    {
        JPH::BodyInterface& bodies = system->GetBodyInterface();
        for (JPH::BodyID body : { record.solid, record.sensor })
        {
            if (!body.IsInvalid())
            {
                owners.erase(body.GetIndexAndSequenceNumber());
                bodies.RemoveBody(body);
                bodies.DestroyBody(body);
            }
        }
    }

    void PhysicsWorld::Implementation::DetectScaleChanges(Level::Level& level)
    {
        for (auto& [id, record] : records)
        {
            Level::LevelObject* object = level.FindObject(id);
            if (!object)
            {
                continue;
            }
            Math::Vector3 scale = object->GetTransform().GetLossyScale();
            if ((scale - record.scale).LengthSquared() > ScaleChangeEpsilon)
            {
                dirty.insert(id);
            }
        }
    }

    void PhysicsWorld::Implementation::RebuildDirtyBodies(Level::Level& level)
    {
        std::unordered_set<Core::Guid> rebuild;
        rebuild.swap(dirty);
        for (const Core::Guid& id : rebuild)
        {
            auto existing = records.find(id);
            if (existing != records.end())
            {
                DestroyBodies(existing->second);
                records.erase(existing);
            }
            Level::LevelObject* object = level.FindObject(id);
            if (object && !object->IsPendingDestroy() && object->IsActiveInHierarchy())
            {
                CreateBodies(*object);
            }
        }
    }

    void PhysicsWorld::Implementation::CreateBodies(Level::LevelObject& object)
    {
        Rigidbody* rigidbody = EnabledRigidbody(object);
        MotionType motion = rigidbody ? rigidbody->GetMotionType() : MotionType::Static;
        BodyShapes shapes = CollectShapes(object, motion);
        if (shapes.solids.empty() && shapes.sensors.empty())
        {
            return;
        }

        JPH::BodyInterface& bodies = system->GetBodyInterface();
        const Level::Transform& transform = object.GetTransform();
        BodyRecord record;
        record.motion = motion;
        record.scale = transform.GetLossyScale();
        if (!shapes.solids.empty())
        {
            record.solid = CreateSolidBody(bodies, shapes, transform, record.scale, motion, rigidbody);
            if (!record.solid.IsInvalid())
            {
                owners[record.solid.GetIndexAndSequenceNumber()] = object.GetId();
            }
            if (rigidbody)
            {
                rigidbody->SetBodyId(record.solid.GetIndexAndSequenceNumber());
            }
        }
        if (!shapes.sensors.empty())
        {
            record.sensor = CreateSensorBody(bodies, shapes, transform, record.scale);
            if (!record.sensor.IsInvalid())
            {
                owners[record.sensor.GetIndexAndSequenceNumber()] = object.GetId();
            }
        }
        record.transformVersion = transform.GetVersion();
        records[object.GetId()] = record;
    }

    void PhysicsWorld::Implementation::MoveBody(JPH::BodyInterface& bodies, JPH::BodyID id, JPH::RVec3Arg position, JPH::QuatArg rotation, bool kinematic, float deltaTime)
    {
        if (kinematic && deltaTime > 0.0f)
        {
            bodies.MoveKinematic(id, position, rotation, deltaTime);
        }
        else
        {
            bodies.SetPositionAndRotation(id, position, rotation, JPH::EActivation::Activate);
        }
    }

    void PhysicsWorld::Implementation::SyncMovedBodies(Level::Level& level, float fixedDeltaTime)
    {
        JPH::BodyInterface& bodies = system->GetBodyInterface();
        for (auto& [id, record] : records)
        {
            Level::LevelObject* object = level.FindObject(id);
            if (!object)
            {
                continue;
            }
            const Level::Transform& transform = object->GetTransform();
            bool moved = transform.GetVersion() != record.transformVersion;
            JPH::RVec3 position = ToJoltPosition(transform.GetPosition());
            JPH::Quat rotation = ToJolt(transform.GetRotation());
            if (!record.solid.IsInvalid() && moved)
            {
                MoveBody(bodies, record.solid, position, rotation, record.motion == MotionType::Kinematic, fixedDeltaTime);
            }
            if (!record.sensor.IsInvalid() && (moved || record.motion == MotionType::Dynamic))
            {
                MoveBody(bodies, record.sensor, position, rotation, true, fixedDeltaTime);
            }
            record.transformVersion = transform.GetVersion();
        }
    }

    void PhysicsWorld::Implementation::ReadBackDynamicBodies(Level::Level& level)
    {
        JPH::BodyInterface& bodies = system->GetBodyInterface();
        for (auto& [id, record] : records)
        {
            if (record.motion != MotionType::Dynamic || record.solid.IsInvalid() || !bodies.IsActive(record.solid))
            {
                continue;
            }
            Level::LevelObject* object = level.FindObject(id);
            if (!object)
            {
                continue;
            }
            JPH::RVec3 position;
            JPH::Quat rotation;
            bodies.GetPositionAndRotation(record.solid, position, rotation);
            object->GetTransform().SetPosition(FromJoltPosition(position));
            object->GetTransform().SetRotation(FromJolt(rotation));
            record.transformVersion = object->GetTransform().GetVersion();
        }
    }
}
