#include "Common/TestUtilities.h"
#include "Level/Level.h"
#include "Physics/Collider.h"
#include "Physics/PhysicsTestTypes.h"
#include "Physics/PhysicsWorld.h"
#include "Physics/Rigidbody.h"

#include <gtest/gtest.h>

using namespace BrightBox3D;
using namespace BrightBox3D::Physics;
using BrightBox3D::Level::LevelObject;

namespace
{
    LevelObject* MakeGround(Level::Level& level)
    {
        LevelObject* ground = *level.CreateObject(LevelObject::StaticType());
        ground->SetName("Ground");
        ground->GetTransform().SetLocalPosition({ 0.0f, -0.5f, 0.0f });
        ground->AddComponent<Collider>()->SetSize({ 40.0f, 1.0f, 40.0f });
        return ground;
    }

    LevelObject* MakeBox(Level::Level& level, const Math::Vector3& position)
    {
        LevelObject* box = *level.CreateObject(LevelObject::StaticType());
        box->SetName("Box");
        box->GetTransform().SetLocalPosition(position);
        EXPECT_TRUE(Tests::Succeeded(box->AddComponent<Collider>()));
        EXPECT_TRUE(Tests::Succeeded(box->AddComponent<Rigidbody>()));
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
    LevelObject* box = MakeBox(level, { 0.0f, 5.0f, 0.0f });
    auto* recorder = *box->AddComponent<Tests::CollisionRecorder>();
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
    LevelObject* zone = *level.CreateObject(LevelObject::StaticType());
    zone->GetTransform().SetLocalPosition({ 0.0f, 3.0f, 0.0f });
    auto* trigger = *zone->AddComponent<Collider>();
    trigger->SetSize({ 4.0f, 1.0f, 4.0f });
    trigger->SetTrigger(true);
    auto* zoneRecorder = *zone->AddComponent<Tests::CollisionRecorder>();
    LevelObject* ball = *level.CreateObject(LevelObject::StaticType());
    ball->GetTransform().SetLocalPosition({ 0.0f, 6.0f, 0.0f });
    auto* ballCollider = *ball->AddComponent<Collider>();
    ballCollider->SetShape(ColliderShape::Sphere);
    ballCollider->SetDiameter(0.5f);
    EXPECT_TRUE(Tests::Succeeded(ball->AddComponent<Rigidbody>()));
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
    LevelObject* ground = MakeGround(level);
    LevelObject* wall = *level.CreateObject(LevelObject::StaticType());
    wall->GetTransform().SetLocalPosition({ 0.0f, 1.0f, 5.0f });
    wall->AddComponent<Collider>()->SetSize({ 4.0f, 2.0f, 0.5f });
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
    LevelObject* box = MakeBox(level, { 0.0f, 0.0f, 0.0f });
    level.BeginPlay();
    level.Update(1.0f / 60.0f);
    auto* body = box->GetComponent<Rigidbody>();
    body->SetLinearVelocity({ 2.0f, 0.0f, 0.0f });
    Simulate(level, 1.0f);
    EXPECT_NEAR(box->GetTransform().GetPosition().x, 2.0f, 0.15f);
    box->GetTransform().SetPosition({ 10.0f, 0.0f, 0.0f });
    level.Update(1.0f / 60.0f);
    EXPECT_GT(box->GetTransform().GetPosition().x, 9.9f);

    LevelObject* spawned = MakeBox(level, { 0.0f, 3.0f, 0.0f });
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
