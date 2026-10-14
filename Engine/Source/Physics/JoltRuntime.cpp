#include "Physics/JoltRuntime.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <thread>

namespace BrightBox3D::Physics
{
    namespace
    {
        constexpr JPH::BroadPhaseLayer StaticBroadPhase(0);
        constexpr JPH::BroadPhaseLayer MovingBroadPhase(1);
        constexpr JPH::uint BroadPhaseLayerCount = 2;
        constexpr JPH::uint MaxBodies = 65536;
        constexpr JPH::uint BodyMutexCount = 0;
        constexpr JPH::uint MaxBodyPairs = 65536;
        constexpr JPH::uint MaxContactConstraints = 16384;
        constexpr size_t TempAllocatorSize = 16 * 1024 * 1024;
        constexpr int ReservedThreads = 1;
        constexpr int MinimumWorkerThreads = 1;
        constexpr int CollisionStepsPerUpdate = 1;
    }

    JPH::uint BroadPhaseLayers::GetNumBroadPhaseLayers() const
    {
        return BroadPhaseLayerCount;
    }

    JPH::BroadPhaseLayer BroadPhaseLayers::GetBroadPhaseLayer(JPH::ObjectLayer layer) const
    {
        return layer == StaticLayer ? StaticBroadPhase : MovingBroadPhase;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* BroadPhaseLayers::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const
    {
        return layer == StaticBroadPhase ? "Static" : "Moving";
    }
#endif

    bool ObjectVsBroadPhaseFilter::ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const
    {
        return layer == MovingLayer || broadPhase == MovingBroadPhase;
    }

    bool ObjectPairFilter::ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const
    {
        return a == MovingLayer || b == MovingLayer;
    }

    JoltRuntime& JoltRuntime::Get()
    {
        static JoltRuntime* runtime = new JoltRuntime();
        return *runtime;
    }

    JoltRuntime::JoltRuntime()
    {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(TempAllocatorSize);
        int threads = std::max(MinimumWorkerThreads, static_cast<int>(std::thread::hardware_concurrency()) - ReservedThreads);
        m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threads);
    }

    std::unique_ptr<JPH::PhysicsSystem> JoltRuntime::CreateSystem() const
    {
        auto system = std::make_unique<JPH::PhysicsSystem>();
        system->Init(MaxBodies, BodyMutexCount, MaxBodyPairs, MaxContactConstraints, m_broadPhaseLayers, m_objectVsBroadPhase, m_objectPair);
        return system;
    }

    void JoltRuntime::Update(JPH::PhysicsSystem& system, float deltaTime)
    {
        system.Update(deltaTime, CollisionStepsPerUpdate, m_tempAllocator.get(), m_jobSystem.get());
    }
}
