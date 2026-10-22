#include "Level/Level.h"
#include "Rendering/Light.h"

#include <gtest/gtest.h>

using namespace BrightBox3D;
using namespace BrightBox3D::Rendering;

namespace
{
    const Reflection::PropertyInfo& Property(const char* name)
    {
        return *Light::StaticType().FindProperty(name);
    }

    Light& MakeLight(Level::Level& level, LightType type)
    {
        Level::LevelObject* object = *level.CreateObject(Level::LevelObject::StaticType());
        Light& light = **object->AddComponent<Light>();
        light.SetLightType(type);
        return light;
    }
}

TEST(LightTests, ShadowResolutionsMapToTileSizes)
{
    EXPECT_EQ(ShadowResolutionSize(ShadowResolution::Low), 512u);
    EXPECT_EQ(ShadowResolutionSize(ShadowResolution::Medium), 1024u);
    EXPECT_EQ(ShadowResolutionSize(ShadowResolution::High), 2048u);
}

TEST(LightTests, NewLightsCastMediumShadows)
{
    Level::Level level;
    Light& light = MakeLight(level, LightType::Point);
    EXPECT_TRUE(light.GetCastShadows());
    EXPECT_EQ(light.GetShadowResolution(), ShadowResolution::Medium);
}

TEST(LightTests, SpotAnglesShowOnlyForSpotLights)
{
    Level::Level level;
    Light& light = MakeLight(level, LightType::Spot);
    EXPECT_TRUE(light.IsPropertyVisible(Property("m_innerAngle")));
    EXPECT_TRUE(light.IsPropertyVisible(Property("m_outerAngle")));
    light.SetLightType(LightType::Point);
    EXPECT_FALSE(light.IsPropertyVisible(Property("m_innerAngle")));
    EXPECT_FALSE(light.IsPropertyVisible(Property("m_outerAngle")));
    light.SetLightType(LightType::Directional);
    EXPECT_FALSE(light.IsPropertyVisible(Property("m_innerAngle")));
}

TEST(LightTests, RangeHiddenForDirectionalLights)
{
    Level::Level level;
    Light& light = MakeLight(level, LightType::Point);
    EXPECT_TRUE(light.IsPropertyVisible(Property("m_range")));
    light.SetLightType(LightType::Spot);
    EXPECT_TRUE(light.IsPropertyVisible(Property("m_range")));
    light.SetLightType(LightType::Directional);
    EXPECT_FALSE(light.IsPropertyVisible(Property("m_range")));
}

TEST(LightTests, LocalShadowOptionsShowForShadowedPointAndSpotLights)
{
    Level::Level level;
    Light& light = MakeLight(level, LightType::Point);
    for (const char* name : { "m_shadowResolution", "m_shadowBias", "m_shadowNormalBias" })
    {
        light.SetLightType(LightType::Point);
        light.SetCastShadows(true);
        EXPECT_TRUE(light.IsPropertyVisible(Property(name))) << name;
        light.SetLightType(LightType::Spot);
        EXPECT_TRUE(light.IsPropertyVisible(Property(name))) << name;
        light.SetCastShadows(false);
        EXPECT_FALSE(light.IsPropertyVisible(Property(name))) << name;
        light.SetLightType(LightType::Directional);
        light.SetCastShadows(true);
        EXPECT_FALSE(light.IsPropertyVisible(Property(name))) << name;
    }
    EXPECT_TRUE(light.IsPropertyVisible(Property("m_castShadows")));
}
