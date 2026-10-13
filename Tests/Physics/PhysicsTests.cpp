#include "Level/Level.h"
#include "Physics/Colliders.h"
#include "Physics/PhysicsWorld.h"
#include "Physics/Rigidbody.h"
#include "Physics/PhysicsTestTypes.h"

#include <gtest/gtest.h>

using namespace BrightBox3D;
using namespace BrightBox3D::Physics;
using BrightBox3D::Level::LevelObjectInstance;

namespace
{
    LevelObjectInstance* MakeGround(Level::Level& level)
    {
        LevelObjectInstance* ground = level.CreateObject(LevelObjectInstance::StaticType());
        ground->SetName("Ground");
        ground->GetTransform().SetLocalPosition({ 0.0f, -0.5f, 0.0f });
        ground->AddComponent<BoxCollider>()->SetSize({ 40.0f, 1.0f, 40.0f });
        return ground;
    }

    LevelObjectInstance* MakeBox(Level::Level& level, const Math::Vector3& position)
    {
        LevelObjectInstance* box = level.CreateObject(LevelObjectInstance::StaticType());
        box->SetName("Box");
        box->GetTransform().SetLocalPosition(position);
        box->AddComponent<BoxCollider>();
        box->AddComponent<Rigidbody>();
        return box;
    }

    void Simulate(Level::Level& level, float seconds)
    {
        int frames = static_cast<int>(seconds * 60.0f);
        for (int i = 0; i < frames; ++i)
        {
            level.Update(1.0f / 60.0f);
        }
    }
}

TEST(PhysicsTests, BodiesFallAndRestOnStaticColliders)
{
    Level::Level level;
    MakeGround(level);
    LevelObjectInstance* box = MakeBox(level, { 0.0f, 5.0f, 0.0f });
    auto* recorder = box->AddComponent<Tests::CollisionRecorder>();
    level.BeginPlay();
    ASSERT_NE(PhysicsWorld::Get(&level), nullptr);
    EXPECT_TRUE(PhysicsWorld::Get(&level)->IsRunning());
    Simulate(level, 0.5f);
    float midAir = box->GetTransform().GetPosition().y;
    EXPECT_LT(midAir, 5.0f);
    EXPECT_GT(midAir, 0.5f);
    Simulate(level, 3.0f);
    EXPECT_NEAR(box->GetTransform().GetPosition().y, 0.5f, 0.05f);
    EXPECT_GE(recorder->collisionEnter, 1);
    EXPECT_NEAR(box->GetComponent<Rigidbody>()->GetLinearVelocity().Length(), 0.0f, 0.1f);
    level.EndPlay();
    EXPECT_FALSE(PhysicsWorld::Get(&level)->IsRunning());
}

TEST(PhysicsTests, TriggersReportEnterAndExit)
{
    Level::Level level;
    LevelObjectInstance* zone = level.CreateObject(LevelObjectInstance::StaticType());
    zone->GetTransform().SetLocalPosition({ 0.0f, 3.0f, 0.0f });
    auto* trigger = zone->AddComponent<BoxCollider>();
    trigger->SetSize({ 4.0f, 1.0f, 4.0f });
    trigger->SetTrigger(true);
    auto* zoneRecorder = zone->AddComponent<Tests::CollisionRecorder>();
    LevelObjectInstance* ball = level.CreateObject(LevelObjectInstance::StaticType());
    ball->GetTransform().SetLocalPosition({ 0.0f, 6.0f, 0.0f });
    ball->AddComponent<SphereCollider>()->SetRadius(0.25f);
    ball->AddComponent<Rigidbody>();
    level.BeginPlay();
    Simulate(level, 2.5f);
    EXPECT_GE(zoneRecorder->triggerEnter, 1);
    EXPECT_GE(zoneRecorder->triggerExit, 1);
    EXPECT_EQ(zoneRecorder->lastOther, ball);
    EXPECT_LT(ball->GetTransform().GetPosition().y, 0.0f);
    level.EndPlay();
}

TEST(PhysicsTests, RaycastsHitCollidersWithNormals)
{
    Level::Level level;
    LevelObjectInstance* ground = MakeGround(level);
    LevelObjectInstance* wall = level.CreateObject(LevelObjectInstance::StaticType());
    wall->GetTransform().SetLocalPosition({ 0.0f, 1.0f, 5.0f });
    wall->AddComponent<BoxCollider>()->SetSize({ 4.0f, 2.0f, 0.5f });
    level.BeginPlay();
    RaycastHit hit;
    ASSERT_TRUE(PhysicsWorld::Raycast(&level, { 0.0f, 10.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 100.0f, hit));
    EXPECT_EQ(hit.object, ground);
    EXPECT_NEAR(hit.distance, 10.0f, 0.01f);
    EXPECT_NEAR(hit.normal.y, 1.0f, 0.01f);
    ASSERT_TRUE(PhysicsWorld::Raycast(&level, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, 100.0f, hit));
    EXPECT_EQ(hit.object, wall);
    EXPECT_NEAR(hit.point.z, 4.75f, 0.01f);
    EXPECT_NEAR(hit.normal.z, -1.0f, 0.01f);
    EXPECT_FALSE(PhysicsWorld::Raycast(&level, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 100.0f, hit));
    level.EndPlay();
}

TEST(PhysicsTests, VelocityApiAndRuntimeChanges)
{
    Level::Level level;
    PhysicsWorld::Get(&level)->SetGravity({ 0.0f, 0.0f, 0.0f });
    LevelObjectInstance* box = MakeBox(level, { 0.0f, 0.0f, 0.0f });
    level.BeginPlay();
    level.Update(1.0f / 60.0f);
    auto* body = box->GetComponent<Rigidbody>();
    body->SetLinearVelocity({ 2.0f, 0.0f, 0.0f });
    Simulate(level, 1.0f);
    EXPECT_NEAR(box->GetTransform().GetPosition().x, 2.0f, 0.15f);
    box->GetTransform().SetPosition({ 10.0f, 0.0f, 0.0f });
    level.Update(1.0f / 60.0f);
    EXPECT_GT(box->GetTransform().GetPosition().x, 9.9f);

    LevelObjectInstance* spawned = MakeBox(level, { 0.0f, 3.0f, 0.0f });
    spawned->GetTransform().SetLocalScale({ 2.0f, 2.0f, 2.0f });
    level.Update(1.0f / 60.0f);
    RaycastHit hit;
    ASSERT_TRUE(PhysicsWorld::Raycast(&level, { 0.0f, 10.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 100.0f, hit));
    EXPECT_EQ(hit.object, spawned);
    EXPECT_NEAR(hit.point.y, 4.0f, 0.05f);
    spawned->Destroy();
    level.Update(1.0f / 60.0f);
    EXPECT_FALSE(PhysicsWorld::Raycast(&level, { 0.0f, 10.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 100.0f, hit));
    level.EndPlay();
}
