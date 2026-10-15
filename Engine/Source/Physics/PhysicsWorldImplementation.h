#pragma once

#include "Core/Guid.h"
#include "Math/Vector3.h"
#include "Physics/PhysicsContacts.h"
#include "Physics/PhysicsWorld.h"
#include "Physics/Rigidbody.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace BrightBox3D::Physics
{
    constexpr Math::Vector3 DefaultGravity{ 0.0f, -9.81f, 0.0f };

    struct PhysicsWorld::Implementation
    {
        struct BodyRecord
        {
            JPH::BodyID solid;
            JPH::BodyID sensor;
            uint64_t transformVersion = 0;
            Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };
            MotionType motion = MotionType::Static;
        };

        std::unique_ptr<JPH::PhysicsSystem> system;
        ContactRecorder contacts;
        std::unordered_map<Core::Guid, BodyRecord> records;
        std::unordered_map<uint32_t, Core::Guid> owners;
        std::unordered_set<Core::Guid> dirty;
        std::set<std::pair<uint32_t, uint32_t>> activePairs;
        Math::Vector3 gravity = DefaultGravity;

        void DestroyBodies(const BodyRecord& record);
        void DetectScaleChanges(Level::Level& level);
        void RebuildDirtyBodies(Level::Level& level);
        void CreateBodies(Level::LevelObject& object);
        void SyncMovedBodies(Level::Level& level, float fixedDeltaTime);
        void ReadBackDynamicBodies(Level::Level& level);
        void DispatchContacts(Level::Level& level);
        JPH::BodyInterface* BodiesFor(const Rigidbody& rigidbody) const;

        static void MoveBody(JPH::BodyInterface& bodies, JPH::BodyID id, JPH::RVec3Arg position, JPH::QuatArg rotation, MotionType motion, float deltaTime);
    };
}
