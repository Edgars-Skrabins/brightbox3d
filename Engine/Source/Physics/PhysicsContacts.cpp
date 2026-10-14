#include "Physics/PhysicsContacts.h"

#include "Level/Level.h"
#include "Physics/CollisionInfo.h"
#include "Physics/JoltConversions.h"
#include "Physics/PhysicsWorldImplementation.h"

#include <Jolt/Physics/Body/Body.h>

#include <algorithm>

namespace BrightBox3D::Physics
{
    void ContactRecorder::OnContactAdded(const JPH::Body& first, const JPH::Body& second, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)
    {
        ContactEvent event;
        event.first = first.GetID();
        event.second = second.GetID();
        event.point = FromJoltPosition(manifold.GetWorldSpaceContactPointOn1(0));
        event.normal = FromJolt(manifold.mWorldSpaceNormal);
        event.relativeVelocity = FromJolt(second.GetLinearVelocity() - first.GetLinearVelocity());
        Record(event);
    }

    void ContactRecorder::OnContactRemoved(const JPH::SubShapeIDPair& pair)
    {
        ContactEvent event;
        event.first = pair.GetBody1ID();
        event.second = pair.GetBody2ID();
        event.added = false;
        Record(event);
    }

    std::vector<ContactEvent> ContactRecorder::Take()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<ContactEvent> events;
        events.swap(m_events);
        return events;
    }

    void ContactRecorder::Record(const ContactEvent& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(event);
    }

    namespace
    {
        std::pair<uint32_t, uint32_t> PairKey(JPH::BodyID first, JPH::BodyID second)
        {
            uint32_t a = first.GetIndexAndSequenceNumber();
            uint32_t b = second.GetIndexAndSequenceNumber();
            return { std::min(a, b), std::max(a, b) };
        }

        void Notify(Reflection::EngineObject& receiver, const CollisionInfo& info, bool added)
        {
            if (info.isTrigger)
            {
                added ? receiver.OnTriggerEnter(info) : receiver.OnTriggerExit(info);
            }
            else
            {
                added ? receiver.OnCollisionEnter(info) : receiver.OnCollisionExit(info);
            }
        }

        void NotifyObject(Level::LevelObject& target, Level::LevelObject& other, const ContactEvent& event, const Math::Vector3& normal, bool trigger)
        {
            CollisionInfo info;
            info.other = &other;
            info.point = event.point;
            info.normal = normal;
            info.relativeVelocity = event.relativeVelocity;
            info.isTrigger = trigger;
            Notify(target, info, event.added);
            for (const auto& component : target.GetComponents())
            {
                if (!component->IsEnabled() || component->IsPendingDestroy())
                {
                    continue;
                }
                Notify(*component, info, event.added);
            }
        }
    }

    void PhysicsWorld::Implementation::DispatchContacts(Level::Level& level)
    {
        for (const ContactEvent& event : contacts.Take())
        {
            auto firstOwner = owners.find(event.first.GetIndexAndSequenceNumber());
            auto secondOwner = owners.find(event.second.GetIndexAndSequenceNumber());
            if (firstOwner == owners.end() || secondOwner == owners.end() || firstOwner->second == secondOwner->second)
            {
                continue;
            }
            std::pair<uint32_t, uint32_t> key = PairKey(event.first, event.second);
            if (event.added)
            {
                if (!activePairs.insert(key).second)
                {
                    continue;
                }
            }
            else if (activePairs.erase(key) == 0)
            {
                continue;
            }
            Level::LevelObject* first = level.FindObject(firstOwner->second);
            Level::LevelObject* second = level.FindObject(secondOwner->second);
            if (!first || !second)
            {
                continue;
            }
            auto isSensor = [&](JPH::BodyID body, const Core::Guid& owner)
            {
                auto record = records.find(owner);
                return record != records.end() && record->second.sensor == body;
            };
            bool trigger = isSensor(event.first, firstOwner->second) || isSensor(event.second, secondOwner->second);
            NotifyObject(*first, *second, event, -event.normal, trigger);
            NotifyObject(*second, *first, event, event.normal, trigger);
        }
    }
}
