#pragma once

#include "Level/LevelSubsystem.h"
#include "Math/Vector3.h"

#include <memory>

namespace BrightBox3D::Level
{
    class Level;
    class LevelObjectInstance;
}

namespace BrightBox3D::Physics
{
    class Rigidbody;

    struct RaycastHit
    {
        Level::LevelObjectInstance* object = nullptr;
        Math::Vector3 point;
        Math::Vector3 normal;
        float distance = 0.0f;
    };

    class PhysicsWorld : public Level::LevelSubsystem
    {
    public:
        explicit PhysicsWorld(Level::Level& level);
        ~PhysicsWorld() override;

        void OnBeginPlay() override;
        void OnEndPlay() override;
        void OnPreFixedUpdate(float fixedDeltaTime) override;
        void OnFixedUpdate(float fixedDeltaTime) override;
        void OnComponentAdded(Level::Component& component) override;
        void OnComponentRemoved(Level::Component& component) override;

        bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& hit, bool includeTriggers = false) const;
        void SetGravity(const Math::Vector3& gravity);
        Math::Vector3 GetGravity() const;
        void MarkDirty(Level::LevelObjectInstance& object);
        bool IsRunning() const;

        Math::Vector3 GetLinearVelocity(const Rigidbody& body) const;
        void SetLinearVelocity(Rigidbody& body, const Math::Vector3& velocity);
        Math::Vector3 GetAngularVelocity(const Rigidbody& body) const;
        void SetAngularVelocity(Rigidbody& body, const Math::Vector3& velocity);
        void AddForce(Rigidbody& body, const Math::Vector3& force);
        void AddImpulse(Rigidbody& body, const Math::Vector3& impulse);
        void AddTorque(Rigidbody& body, const Math::Vector3& torque);
        void MoveKinematic(Rigidbody& body, const Math::Vector3& position, float deltaTime);
        bool IsSleeping(const Rigidbody& body) const;
        void WakeUp(Rigidbody& body);
        void SetGravityScale(Rigidbody& body, float scale);

        static PhysicsWorld* Get(const Level::Level* level);
        static bool Raycast(const Level::Level* level, const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& hit);

    private:
        struct Implementation;
        std::unique_ptr<Implementation> m_implementation;
    };
}
