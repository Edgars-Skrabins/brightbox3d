#include "Common/TestUtilities.h"
#include "Level/Level.h"
#include "Physics/Collider.h"
#include "Physics/PhysicsTestTypes.h"
#include "Physics/PhysicsWorld.h"
#include "Physics/Rigidbody.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/MeshRenderer.h"
#include "Serialization/ObjectSerializer.h"

#include <gtest/gtest.h>

using namespace BrightBox3D;
using namespace BrightBox3D::Physics;
using BrightBox3D::Level::LevelObject;

namespace
{
    const Reflection::PropertyInfo& Property(const char* name)
    {
        return *Collider::StaticType().FindProperty(name);
    }

    void EditShape(Collider& collider, ColliderShape shape)
    {
        const Reflection::PropertyInfo& property = Property("m_shape");
        property.GetValue<ColliderShape>(&collider) = shape;
        collider.OnPropertyEdited(property);
        collider.OnPropertiesChanged();
    }

    Collider& MakeCollider(Level::Level& level)
    {
        LevelObject* object = *level.CreateObject(LevelObject::StaticType());
        return **object->AddComponent<Collider>();
    }
}

TEST(ColliderTests, NewCollidersAreUnitBoxes)
{
    Level::Level level;
    Collider& collider = MakeCollider(level);
    EXPECT_EQ(collider.GetShape(), ColliderShape::Box);
    EXPECT_EQ(collider.GetSize(), Math::Vector3(1.0f, 1.0f, 1.0f));
    EXPECT_FLOAT_EQ(collider.GetDiameter(), 1.0f);
}

TEST(ColliderTests, SwitchingShapesCarriesSizes)
{
    Level::Level level;
    Collider& collider = MakeCollider(level);
    collider.SetSize({ 2.0f, 3.0f, 4.0f });
    collider.SetShape(ColliderShape::Sphere);
    EXPECT_FLOAT_EQ(collider.GetDiameter(), 2.0f);
    collider.SetDiameter(5.0f);
    collider.SetShape(ColliderShape::Box);
    EXPECT_EQ(collider.GetSize(), Math::Vector3(5.0f, 5.0f, 5.0f));
    collider.SetSize({ 2.0f, 3.0f, 4.0f });
    collider.SetShape(ColliderShape::Mesh);
    EXPECT_EQ(collider.GetSize(), Math::Vector3(1.0f, 1.0f, 1.0f));
    collider.SetSize({ 2.0f, 3.0f, 4.0f });
    collider.SetShape(ColliderShape::Box);
    EXPECT_EQ(collider.GetSize(), Math::Vector3(2.0f, 3.0f, 4.0f));
}

TEST(ColliderTests, InspectorEditsConvertButLoadingKeepsValues)
{
    Level::Level level;
    Collider& edited = MakeCollider(level);
    edited.SetSize({ 3.0f, 1.0f, 1.0f });
    EditShape(edited, ColliderShape::Sphere);
    EXPECT_FLOAT_EQ(edited.GetDiameter(), 3.0f);
    EditShape(edited, ColliderShape::Mesh);
    EXPECT_EQ(edited.GetSize(), Math::Vector3(1.0f, 1.0f, 1.0f));

    Collider source;
    source.SetShape(ColliderShape::Mesh);
    source.SetSize({ 2.0f, 2.0f, 2.0f });
    Serialization::JsonDocument document;
    Serialization::SerializeProperties(source, document, document.GetAllocator());
    Collider& loaded = MakeCollider(level);
    Serialization::DeserializeProperties(loaded, document);
    EXPECT_EQ(loaded.GetShape(), ColliderShape::Mesh);
    EXPECT_EQ(loaded.GetSize(), Math::Vector3(2.0f, 2.0f, 2.0f));
}

TEST(ColliderTests, OnlyTheActiveShapesFieldsAreVisible)
{
    Level::Level level;
    Collider& collider = MakeCollider(level);
    EXPECT_TRUE(collider.IsPropertyVisible(Property("m_size")));
    EXPECT_FALSE(collider.IsPropertyVisible(Property("m_diameter")));
    EXPECT_FALSE(collider.IsPropertyVisible(Property("m_model")));
    collider.SetShape(ColliderShape::Sphere);
    EXPECT_FALSE(collider.IsPropertyVisible(Property("m_size")));
    EXPECT_TRUE(collider.IsPropertyVisible(Property("m_diameter")));
    collider.SetShape(ColliderShape::Mesh);
    EXPECT_TRUE(collider.IsPropertyVisible(Property("m_size")));
    EXPECT_TRUE(collider.IsPropertyVisible(Property("m_model")));
    EXPECT_TRUE(collider.IsPropertyVisible(Property("m_meshIndex")));
    EXPECT_TRUE(collider.IsPropertyVisible(Property("m_convex")));
    EXPECT_TRUE(collider.IsPropertyVisible(Property("m_isTrigger")));
}

TEST(ColliderTests, MeshShapeUsesTheObjectsRendererUnlessAModelIsSet)
{
    Level::Level level;
    LevelObject* object = *level.CreateObject(LevelObject::StaticType());
    auto* renderer = *object->AddComponent<Rendering::MeshRenderer>();
    renderer->SetModel(Assets::AssetReference<Geometry::ModelAsset>(Rendering::BuiltinAssets::Sphere()));
    renderer->SetMeshIndex(0);
    auto* collider = *object->AddComponent<Collider>();
    collider->SetShape(ColliderShape::Mesh);
    EXPECT_EQ(collider->ResolveMesh().model.guid, Rendering::BuiltinAssets::Sphere());
    collider->SetMesh(Assets::AssetReference<Geometry::ModelAsset>(Rendering::BuiltinAssets::Cube()), 0);
    EXPECT_EQ(collider->ResolveMesh().model.guid, Rendering::BuiltinAssets::Cube());
}

TEST(ColliderTests, ScaledMeshGroundStopsAFallingBox)
{
    Level::Level level;
    LevelObject* ground = *level.CreateObject(LevelObject::StaticType());
    ground->GetTransform().SetLocalPosition({ 0.0f, -0.5f, 0.0f });
    (*ground->AddComponent<Rendering::MeshRenderer>())->SetModel(Assets::AssetReference<Geometry::ModelAsset>(Rendering::BuiltinAssets::Cube()));
    auto* groundCollider = *ground->AddComponent<Collider>();
    groundCollider->SetShape(ColliderShape::Mesh);
    groundCollider->SetSize({ 40.0f, 1.0f, 40.0f });
    LevelObject* box = *level.CreateObject(LevelObject::StaticType());
    box->GetTransform().SetLocalPosition({ 5.0f, 3.0f, 0.0f });
    EXPECT_TRUE(Tests::Succeeded(box->AddComponent<Collider>()));
    EXPECT_TRUE(Tests::Succeeded(box->AddComponent<Rigidbody>()));
    level.BeginPlay();
    for (int frame = 0; frame < 180; ++frame)
    {
        level.Update(1.0f / 60.0f);
    }
    EXPECT_NEAR(box->GetTransform().GetPosition().y, 0.5f, 0.05f);
    level.EndPlay();
}
