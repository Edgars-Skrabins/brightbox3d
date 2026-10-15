#include "Assets/AssetDatabase.h"
#include "Common/TestUtilities.h"
#include "Framework/Camera.h"
#include "Level/Level.h"
#include "Level/LevelAssets.h"
#include "Level/LevelSerializer.h"
#include "Level/LevelTestTypes.h"
#include "Runtime/LevelObjectAssetMigration.h"
#include "Runtime/Project.h"
#include "Serialization/Json.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace BrightBox3D;
using namespace BrightBox3D::Tests;
using BrightBox3D::Level::LevelObject;
using BrightBox3D::Level::LevelObjectLibrary;
using BrightBox3D::Level::LevelObjectVariantAsset;
using BrightBox3D::Level::LevelSerializer;
using RuntimeLevel = BrightBox3D::Level::Level;

namespace
{
    class VariantProject
    {
    public:
        VariantProject()
        {
            m_root = Tests::MakeTempDirectory("Variants");
            std::filesystem::create_directories(m_root / "Assets");
        }

        ~VariantProject()
        {
            Assets::AssetDatabase::Get().Close();
            LevelObjectLibrary::Get().Clear();
            std::error_code error;
            std::filesystem::remove_all(m_root, error);
        }

        void Open() { Assets::AssetDatabase::Get().Open(m_root); }
        const std::filesystem::path& Root() const { return m_root; }

    private:
        std::filesystem::path m_root;
    };

    Core::Guid CreateTemplate(int lives, float moveSpeed)
    {
        RuntimeLevel authoring;
        auto* source = static_cast<TestPlayer*>(authoring.CreateObject(TestPlayer::StaticType()));
        source->SetName("Enemy");
        source->m_lives = lives;
        source->m_moveSpeed = moveSpeed;
        Serialization::JsonDocument document;
        LevelSerializer::MakeLevelObjectDocument(*source, document, Assets::AssetType::LevelObjectTemplate);
        return Assets::AssetDatabase::Get().CreateJsonAsset("Enemy.template", document);
    }

    Core::Guid CreateVariant(const Core::Guid& parent, int lives, const std::string& path)
    {
        Serialization::JsonDocument linked;
        linked.SetObject();
        linked.AddMember("levelObject", Serialization::MakeString(parent.ToString(), linked.GetAllocator()), linked.GetAllocator());
        RuntimeLevel scratch;
        auto* variant = static_cast<TestPlayer*>(LevelSerializer::ReadObject(scratch, nullptr, nullptr, linked, Level::Initialization::Deferred));
        if (!variant)
        {
            return {};
        }
        variant->m_lives = lives;
        Serialization::JsonDocument document;
        LevelSerializer::MakeLevelObjectDocument(*variant, document, Assets::AssetType::LevelObjectVariant);
        return Assets::AssetDatabase::Get().CreateJsonAsset(path, document);
    }

    void EditAsset(const Core::Guid& asset, const std::function<void(TestPlayer&)>& change)
    {
        RuntimeLevel editing;
        auto* root = static_cast<TestPlayer*>(LevelSerializer::OpenAsset(editing, nullptr, asset));
        ASSERT_NE(root, nullptr);
        change(*root);
        ASSERT_TRUE(LevelSerializer::SaveAsset(*root, asset));
    }
}

TEST(VariantTests, TemplateEditsReachVariantsExceptOverrides)
{
    VariantProject project;
    project.Open();
    Core::Guid templateAsset = CreateTemplate(3, 600.0f);
    Core::Guid variant = CreateVariant(templateAsset, 5, "Brute.variant");
    ASSERT_TRUE(variant.IsValid());
    EXPECT_EQ(Assets::AssetDatabase::Get().Load<LevelObjectVariantAsset>(variant)->GetParentTemplate(), templateAsset);

    EditAsset(templateAsset, [](TestPlayer& root)
    {
        root.m_lives = 9;
        root.m_moveSpeed = 700.0f;
    });
    LevelObjectLibrary::Get().Clear();

    RuntimeLevel level;
    auto* placed = static_cast<TestPlayer*>(LevelSerializer::Instantiate(level, variant, nullptr, nullptr));
    ASSERT_NE(placed, nullptr);
    EXPECT_EQ(placed->m_lives, 5);
    EXPECT_FLOAT_EQ(placed->m_moveSpeed, 700.0f);
}

TEST(VariantTests, VariantEditsReachPlacedInstancesExceptOverrides)
{
    VariantProject project;
    project.Open();
    Core::Guid variant = CreateVariant(CreateTemplate(3, 600.0f), 5, "Brute.variant");
    RuntimeLevel level;
    auto* placed = static_cast<TestPlayer*>(LevelSerializer::Instantiate(level, variant, nullptr, nullptr));
    ASSERT_NE(placed, nullptr);
    Core::Guid id = placed->GetId();
    placed->m_lives = 11;

    EditAsset(variant, [](TestPlayer& root) { root.m_moveSpeed = 800.0f; });
    LevelSerializer::RefreshLinkedInstances(level, variant);

    auto* refreshed = static_cast<TestPlayer*>(level.FindObject(id));
    ASSERT_NE(refreshed, nullptr);
    EXPECT_EQ(refreshed->m_lives, 11);
    EXPECT_FLOAT_EQ(refreshed->m_moveSpeed, 800.0f);
    EXPECT_TRUE(LevelSerializer::IsPropertyOverridden(*refreshed, nullptr, "m_lives"));
    EXPECT_FALSE(LevelSerializer::IsPropertyOverridden(*refreshed, nullptr, "m_moveSpeed"));
}

TEST(VariantTests, SpawnReturnsLinkedObjects)
{
    VariantProject project;
    project.Open();
    Core::Guid variant = CreateVariant(CreateTemplate(3, 600.0f), 5, "Brute.variant");
    RuntimeLevel level;
    Assets::AssetReference<LevelObjectVariantAsset> reference(variant);
    auto* spawned = level.Spawn<TestPlayer>(reference, { 1.0f, 2.0f, 3.0f }, Math::Quaternion());
    ASSERT_NE(spawned, nullptr);
    EXPECT_TRUE(spawned->GetLink().isRoot);
    EXPECT_EQ(spawned->GetLink().asset, variant);
    EXPECT_EQ(spawned->m_lives, 5);
    EXPECT_FLOAT_EQ(spawned->GetTransform().GetPosition().y, 2.0f);
    EXPECT_EQ(level.Spawn<Framework::Camera>(reference, {}, Math::Quaternion()), nullptr);
}

TEST(VariantTests, TemplatesAndVariantsOfVariantsAreRefused)
{
    VariantProject project;
    project.Open();
    Core::Guid templateAsset = CreateTemplate(3, 600.0f);
    Core::Guid variant = CreateVariant(templateAsset, 5, "Brute.variant");
    RuntimeLevel level;
    EXPECT_EQ(LevelSerializer::Instantiate(level, templateAsset, nullptr, nullptr), nullptr);
    EXPECT_EQ(level.Spawn(Assets::AssetReference<LevelObjectVariantAsset>(templateAsset), {}, Math::Quaternion()), nullptr);
    Core::Guid nested = CreateVariant(variant, 7, "Nested.variant");
    ASSERT_TRUE(nested.IsValid());
    EXPECT_EQ(LevelSerializer::Instantiate(level, nested, nullptr, nullptr), nullptr);
}

TEST(VariantTests, OldLevelObjectFilesBecomeVariants)
{
    VariantProject project;
    std::filesystem::path assets = project.Root() / "Assets";
    Core::Guid guid = Core::Guid::Generate();
    RuntimeLevel authoring;
    LevelObject* source = authoring.CreateObject(TestPlayer::StaticType());
    Serialization::JsonDocument document;
    LevelSerializer::MakeLevelObjectDocument(*source, document, Assets::AssetType::LevelObjectVariant);
    Serialization::SetMember(document, "$asset", Serialization::JsonValue("LevelObject", document.GetAllocator()), document.GetAllocator());
    ASSERT_TRUE(Serialization::SaveJsonFile(assets / "Old.levelobject", document));
    Serialization::JsonDocument meta;
    meta.SetObject();
    meta.AddMember("guid", Serialization::MakeString(guid.ToString(), meta.GetAllocator()), meta.GetAllocator());
    meta.AddMember("type", "LevelObject", meta.GetAllocator());
    ASSERT_TRUE(Serialization::SaveJsonFile(assets / "Old.levelobject.meta", meta));

    Runtime::Project fake;
    std::string error;
    Runtime::Project::Create(project.Root(), "Variants", error);
    ASSERT_TRUE(fake.Open(project.Root(), error)) << error;
    EXPECT_EQ(Runtime::MigrateLegacyLevelObjectAssets(fake), 1);
    EXPECT_FALSE(std::filesystem::exists(assets / "Old.levelobject"));
    EXPECT_TRUE(std::filesystem::exists(assets / "Old.variant"));
    project.Open();
    const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(guid);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->type, Assets::AssetType::LevelObjectVariant);
    RuntimeLevel level;
    EXPECT_NE(LevelSerializer::Instantiate(level, guid, nullptr, nullptr), nullptr);
}
