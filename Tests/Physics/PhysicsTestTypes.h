#pragma once

#include "Level/LevelObjectComponent.h"
#include "Physics/CollisionInfo.h"

namespace BrightBox3D::Tests
{
    BrightClass()
    class CollisionRecorder : public Level::LevelObjectComponent
    {
        BrightBody()
    public:
        void OnCollisionEnter(const Physics::CollisionInfo& collision) override { ++collisionEnter; }
        void OnCollisionExit(const Physics::CollisionInfo& collision) override { ++collisionExit; }
        void OnTriggerEnter(const Physics::CollisionInfo& collision) override { ++triggerEnter; lastOther = collision.other; }
        void OnTriggerExit(const Physics::CollisionInfo& collision) override { ++triggerExit; }

        int collisionEnter = 0;
        int collisionExit = 0;
        int triggerEnter = 0;
        int triggerExit = 0;
        Level::LevelObject* lastOther = nullptr;
    };
}
