#include "Reflection/TestTypes.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/BinaryJson.h"
#include "Serialization/ObjectSerializer.h"

#include <gtest/gtest.h>

using namespace BrightBox3D;
using namespace BrightBox3D::Reflection;
using namespace BrightBox3D::Serialization;
using namespace BrightBox3D::Tests;

TEST(ReflectionTests, RegistersTypesWithHierarchy)
{
    const TypeInfo* derived = TypeRegistry::Find("TestDerived");
    const TypeInfo* base = TypeRegistry::Find("TestBase");
    ASSERT_NE(derived, nullptr);
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(derived, &TestDerived::StaticType());
    EXPECT_EQ(derived->GetBase(), base);
    EXPECT_TRUE(derived->IsA(EngineObject::StaticType()));
    EXPECT_TRUE(base->IsAbstract());
    EXPECT_FALSE(derived->IsAbstract());
    EXPECT_EQ(derived->GetCategory(), "Testing");
}

TEST(ReflectionTests, CollectsPropertiesWithFlags)
{
    const TypeInfo& type = TestDerived::StaticType();
    auto properties = type.GetAllProperties();
    ASSERT_EQ(properties.size(), 12u);
    EXPECT_EQ(properties.front()->name, "m_baseValue");
    EXPECT_EQ(type.FindProperty("m_notReflected"), nullptr);

    const PropertyInfo* speed = type.FindProperty("m_moveSpeed");
    ASSERT_NE(speed, nullptr);
    EXPECT_EQ(speed->displayName, "Move Speed");
    EXPECT_EQ(speed->category, "Movement");
    EXPECT_TRUE(speed->HasFlag(PropertyFlagEditDefaultsOnly));
    EXPECT_EQ(speed->value.kind, PropertyKind::Float);

    const PropertyInfo* health = type.FindProperty("m_health");
    EXPECT_TRUE(health->HasFlag(PropertyFlagUIRead));
    EXPECT_FALSE(health->HasFlag(PropertyFlagUIWrite));
    EXPECT_TRUE(health->HasFlag(PropertyFlagVisibleAnywhere));

    const PropertyInfo* name = type.FindProperty("m_playerName");
    EXPECT_TRUE(name->HasFlag(PropertyFlagUIWrite));
    EXPECT_TRUE(name->HasFlag(PropertyFlagUIRead));

    const PropertyInfo* mode = type.FindProperty("m_mode");
    EXPECT_EQ(mode->value.kind, PropertyKind::Enum);
    ASSERT_NE(mode->value.enumInfo, nullptr);
    EXPECT_EQ(mode->value.enumInfo->entries.size(), 3u);
    EXPECT_EQ(mode->value.enumInfo->FindByName("Running")->value, 6);
    EXPECT_NE(TypeRegistry::FindEnum("TestMode"), nullptr);

    const PropertyInfo* references = type.FindProperty("m_references");
    EXPECT_EQ(references->value.kind, PropertyKind::Array);
    EXPECT_EQ(references->element.kind, PropertyKind::AssetReference);
    EXPECT_EQ(references->element.assetType, Assets::AssetType::Texture);
}

TEST(ReflectionTests, AccessesValuesThroughProperties)
{
    TestDerived object;
    const PropertyInfo* health = TestDerived::StaticType().FindProperty("m_health");
    health->GetValue<int>(&object) = 42;
    EXPECT_EQ(object.GetHealth(), 42);
}

TEST(ReflectionTests, CreatesInstancesByName)
{
    std::unique_ptr<EngineObject> object(TypeRegistry::Find("TestDerived")->CreateInstance());
    ASSERT_NE(object, nullptr);
    EXPECT_NE(Cast<TestDerived>(object.get()), nullptr);
    EXPECT_EQ(TypeRegistry::Find("TestBase")->CreateInstance(), nullptr);
}

TEST(SerializationTests, RoundTripsThroughJson)
{
    TestDerived source;
    source.SetHealth(5);
    source.m_mode = TestMode::Running;
    source.m_offset = { -1.5f, 0.1f, 99.0f };
    source.m_rotation = Math::Quaternion::Euler(10.0f, 20.0f, 30.0f);
    source.m_weights = { 0.1f, 0.2f, 0.3f };
    source.m_reference.guid = Core::Guid::Generate();
    source.m_references.push_back({ Core::Guid::Generate() });
    source.m_flag = true;

    JsonDocument document;
    SerializeObject(source, document, document.GetAllocator());
    std::string text = WriteJson(document);
    EXPECT_NE(text.find("\"$type\": \"TestDerived\""), std::string::npos);
    EXPECT_NE(text.find("\"m_mode\": \"Running\""), std::string::npos);
    EXPECT_NE(text.find("0.1"), std::string::npos);

    JsonDocument parsed;
    ASSERT_TRUE(ParseJson(text, parsed));
    std::string error;
    std::unique_ptr<EngineObject> object = DeserializeObject(parsed, &error);
    ASSERT_NE(object, nullptr) << error;
    auto* restored = Cast<TestDerived>(object.get());
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->GetHealth(), 5);
    EXPECT_EQ(restored->m_mode, TestMode::Running);
    EXPECT_EQ(restored->m_offset, source.m_offset);
    EXPECT_EQ(restored->m_rotation, source.m_rotation);
    EXPECT_EQ(restored->m_weights, source.m_weights);
    EXPECT_EQ(restored->m_reference.guid, source.m_reference.guid);
    ASSERT_EQ(restored->m_references.size(), 1u);
    EXPECT_EQ(restored->m_references[0].guid, source.m_references[0].guid);
    EXPECT_TRUE(restored->m_flag);
}

TEST(SerializationTests, BinaryJsonRoundTrip)
{
    TestDerived source;
    source.m_playerName = "Bright";
    JsonDocument document;
    SerializeObject(source, document, document.GetAllocator());
    document.AddMember("negative", -123456789012345ll, document.GetAllocator());
    document.AddMember("big", 18446744073709551615ull, document.GetAllocator());

    std::vector<uint8_t> bytes = EncodeBinaryJson(document);
    JsonDocument decoded;
    ASSERT_TRUE(DecodeBinaryJson(bytes.data(), bytes.size(), decoded));
    EXPECT_TRUE(decoded == document);
    EXPECT_FALSE(DecodeBinaryJson(bytes.data(), bytes.size() / 2, decoded));
}

TEST(SerializationTests, ComparesAndCopiesProperties)
{
    TestDerived a;
    TestDerived b;
    const PropertyInfo* speed = TestDerived::StaticType().FindProperty("m_moveSpeed");
    EXPECT_TRUE(PropertyValuesEqual(*speed, a, b));
    a.m_moveSpeed = 1.0f;
    EXPECT_FALSE(PropertyValuesEqual(*speed, a, b));
    CopyPropertyValue(*speed, a, b);
    EXPECT_EQ(b.m_moveSpeed, 1.0f);
}
