#pragma once

#include "Math/Vector3.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/ContactListener.h>

#include <mutex>
#include <vector>

namespace BrightBox3D::Physics
{
    struct ContactEvent
    {
        JPH::BodyID first;
        JPH::BodyID second;
        Math::Vector3 point;
        Math::Vector3 normal;
        Math::Vector3 relativeVelocity;
        bool added = true;
    };

    class ContactRecorder final : public JPH::ContactListener
    {
    public:
        void OnContactAdded(const JPH::Body& first, const JPH::Body& second, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override;
        void OnContactRemoved(const JPH::SubShapeIDPair& pair) override;

        std::vector<ContactEvent> Take();

    private:
        void Record(const ContactEvent& event);

        std::mutex m_mutex;
        std::vector<ContactEvent> m_events;
    };
}
