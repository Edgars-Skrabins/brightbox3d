#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <memory>

namespace BrightBox3D::Physics
{
    constexpr JPH::ObjectLayer StaticLayer = 0;
    constexpr JPH::ObjectLayer MovingLayer = 1;

    class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface
    {
    public:
        JPH::uint GetNumBroadPhaseLayers() const override;
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif
    };

    class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const override;
    };

    class ObjectPairFilter final : public JPH::ObjectLayerPairFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override;
    };

    class JoltRuntime
    {
    public:
        static JoltRuntime& Get();

        std::unique_ptr<JPH::PhysicsSystem> CreateSystem() const;
        void Update(JPH::PhysicsSystem& system, float deltaTime);

    private:
        JoltRuntime();

        std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
        std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;
        BroadPhaseLayers m_broadPhaseLayers;
        ObjectVsBroadPhaseFilter m_objectVsBroadPhase;
        ObjectPairFilter m_objectPair;
    };
}
