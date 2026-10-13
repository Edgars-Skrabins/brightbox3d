#pragma once

#include "Level/Component.h"

namespace BrightBox3D::Tests
{
    BrightClass()
    class CollisionRecorder : public Level::Component
    {
        BrightBody()
    public:
        void OnCollisionEnter(const Level::CollisionInfo& collision) override { ++collisionEnter; }
        void OnCollisionExit(const Level::CollisionInfo& collision) override { ++collisionExit; }
        void OnTriggerEnter(const Level::CollisionInfo& collision) override { ++triggerEnter; lastOther = collision.other; }
        void OnTriggerExit(const Level::CollisionInfo& collision) override { ++triggerExit; }

        int collisionEnter = 0;
        int collisionExit = 0;
        int triggerEnter = 0;
        int triggerExit = 0;
        Level::LevelObjectInstance* lastOther = nullptr;
    };
}
