#include "Editor/EditorTestTypes.h"
#include "Level/Level.h"
#include "ObjectIcons.h"
#include "Rendering/Light.h"
#include "Rendering/MeshRenderer.h"

#include <gtest/gtest.h>

using namespace BrightBox3D;
using BrightBox3D::Core::Icon;
using BrightBox3D::Editor::ObjectIcons;

TEST(ObjectIconTests, ClassIconWinsOverComponentIcon)
{
    Level::Level level;
    Level::LevelObject* start = level.CreateObject<Framework::PlayerStart>();
    start->AddComponent<Rendering::MeshRenderer>();
    EXPECT_EQ(ObjectIcons::ResolveObjectIcon(*start), Icon::Flag);
}

TEST(ObjectIconTests, ClassIconsAreInherited)
{
    Level::Level level;
    EXPECT_EQ(ObjectIcons::ResolveObjectIcon(*level.CreateObject<Tests::TestSpawnPoint>()), Icon::Flag);
    EXPECT_EQ(ObjectIcons::TypeIcon(Tests::TestSpawnPoint::StaticType()), Icon::Flag);
}

TEST(ObjectIconTests, FirstComponentWithAnIconIsUsed)
{
    Level::Level level;
    Level::LevelObject* object = level.CreateObject(Level::LevelObject::StaticType());
    object->AddComponent<Rendering::Light>();
    object->AddComponent<Rendering::MeshRenderer>();
    EXPECT_EQ(ObjectIcons::ResolveObjectIcon(*object), Icon::Lightbulb);
}

TEST(ObjectIconTests, GameClassesWithoutIconsFallBack)
{
    Level::Level level;
    EXPECT_EQ(ObjectIcons::ResolveObjectIcon(*level.CreateObject<Tests::TestGameObject>()), Icon::FileCode);
    EXPECT_EQ(ObjectIcons::ResolveObjectIcon(*level.CreateObject(Level::LevelObject::StaticType())), Icon::CircleDashed);
}

TEST(ObjectIconTests, CacheRefreshesWhenComponentsChange)
{
    Level::Level level;
    ObjectIcons icons;
    Level::LevelObject* object = level.CreateObject(Level::LevelObject::StaticType());
    EXPECT_EQ(icons.ObjectIcon(*object), Icon::CircleDashed);
    object->AddComponent<Rendering::MeshRenderer>();
    EXPECT_EQ(icons.ObjectIcon(*object), Icon::Box);
}
