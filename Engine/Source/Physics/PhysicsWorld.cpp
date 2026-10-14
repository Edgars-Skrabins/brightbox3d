#include "Physics/PhysicsWorld.h"

#include "Level/Level.h"
#include "Physics/Colliders.h"
#include "Physics/JoltConversions.h"
#include "Physics/JoltRuntime.h"
#include "Physics/PhysicsWorldImplementation.h"
#include "Physics/Rigidbody.h"

namespace BrightBox3D::Physics
{
    static_assert(Rigidbody::InvalidBodyId == JPH::BodyID::cInvalidBodyID);

    namespace
    {
        bool IsPhysicsComponent(const Level::LevelObjectComponent& component)
        {
            return component.IsA<Collider>() || component.IsA<Rigidbody>();
        }

        bool HasPhysicsComponent(Level::LevelObject& object)
        {
            return object.GetComponent<Collider>() || object.GetComponent<Rigidbody>();
        }
    }

    PhysicsWorld::PhysicsWorld(Level::Level& level) : LevelSubsystem(level), m_implementation(std::make_unique<Implementation>())
    {
    }

    PhysicsWorld::~PhysicsWorld()
    {
        OnGameEnd();
    }

    PhysicsWorld* PhysicsWorld::Get(const Level::Level* level)
    {
        return level ? level->GetSubsystem<PhysicsWorld>() : nullptr;
    }

    bool PhysicsWorld::IsRunning() const
    {
        return m_implementation->system != nullptr;
    }

    void PhysicsWorld::OnGameStart()
    {
        std::unique_ptr<JPH::PhysicsSystem> system = JoltRuntime::Get().CreateSystem();
        system->SetGravity(ToJolt(m_implementation->gravity));
        system->SetContactListener(&m_implementation->contacts);
        m_implementation->system = std::move(system);
        GetLevel()->ForEachObject([&](Level::LevelObject& object)
        {
            if (HasPhysicsComponent(object))
            {
                m_implementation->dirty.insert(object.GetId());
            }
        });
        OnPreFixedUpdate(0.0f);
        m_implementation->system->OptimizeBroadPhase();
    }

    void PhysicsWorld::OnGameEnd()
    {
        if (!m_implementation->system)
        {
            return;
        }
        for (auto& [id, record] : m_implementation->records)
        {
            m_implementation->DestroyBodies(record);
        }
        m_implementation->records.clear();
        m_implementation->owners.clear();
        m_implementation->dirty.clear();
        m_implementation->activePairs.clear();
        m_implementation->system.reset();
    }

    void PhysicsWorld::OnGameUpdateFixed(float fixedDeltaTime)
    {
        if (!m_implementation->system || fixedDeltaTime <= 0.0f)
        {
            return;
        }
        JoltRuntime::Get().Update(*m_implementation->system, fixedDeltaTime);
        m_implementation->ReadBackDynamicBodies(*GetLevel());
        m_implementation->DispatchContacts(*GetLevel());
    }

    void PhysicsWorld::MarkDirty(Level::LevelObject& object)
    {
        m_implementation->dirty.insert(object.GetId());
    }

    void PhysicsWorld::OnComponentAdded(Level::LevelObjectComponent& component)
    {
        if (IsPhysicsComponent(component))
        {
            MarkDirty(*component.GetOwner());
        }
    }

    void PhysicsWorld::OnComponentRemoved(Level::LevelObjectComponent& component)
    {
        if (!IsPhysicsComponent(component))
        {
            return;
        }
        Core::Guid id = component.GetOwner()->GetId();
        m_implementation->dirty.insert(id);
        if (auto* rigidbody = Reflection::Cast<Rigidbody>(&component))
        {
            rigidbody->SetBodyId(Rigidbody::InvalidBodyId);
        }
        auto it = m_implementation->records.find(id);
        if (it != m_implementation->records.end() && m_implementation->system)
        {
            m_implementation->DestroyBodies(it->second);
            m_implementation->records.erase(it);
        }
    }

    void PhysicsWorld::SetGravity(const Math::Vector3& gravity)
    {
        m_implementation->gravity = gravity;
        if (m_implementation->system)
        {
            m_implementation->system->SetGravity(ToJolt(gravity));
        }
    }

    Math::Vector3 PhysicsWorld::GetGravity() const
    {
        return m_implementation->gravity;
    }

    namespace
    {
        struct Registration
        {
            Registration()
            {
                Level::LevelSubsystemRegistry::Register("Physics", [](Level::Level& level) { return std::make_unique<PhysicsWorld>(level); });
            }
        };

        Registration s_registration;
    }
}
