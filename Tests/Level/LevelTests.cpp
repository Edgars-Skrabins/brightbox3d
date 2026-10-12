#include "Assets/AssetDatabase.h"
#include "Level/Level.h"
#include "Level/LevelAssets.h"
#include "Level/LevelSerializer.h"
#include "Level/MissingComponent.h"
#include "Level/LevelTestTypes.h"
#include "Serialization/ObjectSerializer.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace BrightBox3D;
using BrightBox3D::Level::LevelObjectInstance;
using BrightBox3D::Level::LevelCollection;
using BrightBox3D::Level::LevelSerializer;
using BrightBox3D::Level::LevelObjectLibrary;
using BrightBox3D::Level::LevelCollectionAsset;
using BrightBox3D::Level::MissingComponent;

using namespace BrightBox3D::Tests;
using RuntimeLevel = BrightBox3D::Level::Level;

namespace BrightBox3D::Tests
{
    std::vector<std::string>& LifecycleLog()
    {
        static std::vector<std::string> log;
        return log;
    }
}

namespace
{
    bool Contains(const std::vector<std::string>& log, const std::string& entry)
    {
        return std::find(log.begin(), log.end(), entry) != log.end();
    }

    int IndexOf(const std::vector<std::string>& log, const std::string& entry)
    {
        auto it = std::find(log.begin(), log.end(), entry);
        return it == log.end() ? -1 : static_cast<int>(it - log.begin());
    }

    void ExpectNear(const Math::Vector3& a, const Math::Vector3& b)
    {
        EXPECT_NEAR(a.x, b.x, 1.0e-4f);
        EXPECT_NEAR(a.y, b.y, 1.0e-4f);
        EXPECT_NEAR(a.z, b.z, 1.0e-4f);
    }

    class TempProject
    {
    public:
        TempProject()
        {
            m_root = std::filesystem::temp_directory_path() / ("BrightBox3DProject_" + Core::Guid::Generate().ToString());
            std::filesystem::create_directories(m_root / "Assets");
            Assets::AssetDatabase::Get().Open(m_root);
        }

        ~TempProject()
        {
            Assets::AssetDatabase::Get().Close();
            LevelObjectLibrary::Get().Clear();
            std::error_code error;
            std::filesystem::remove_all(m_root, error);
        }

        const std::filesystem::path& Root() const { return m_root; }

    private:
        std::filesystem::path m_root;
    };
}

TEST(LevelTests, TransformHierarchyComposesAndKeepsWorld)
{
    RuntimeLevel level;
    LevelObjectInstance* parent = level.CreateObject(LevelObjectInstance::StaticType());
    LevelObjectInstance* child = level.CreateObject(LevelObjectInstance::StaticType(), nullptr, parent);
    parent->GetTransform().SetLocalPosition({ 10.0f, 0.0f, 0.0f });
    parent->GetTransform().SetLocalEulerAngles({ 0.0f, 90.0f, 0.0f });
    child->GetTransform().SetLocalPosition({ 0.0f, 0.0f, 1.0f });
    ExpectNear(child->GetTransform().GetPosition(), { 11.0f, 0.0f, 0.0f });
    ExpectNear(child->GetTransform().GetForward(), { 1.0f, 0.0f, 0.0f });

    child->SetParent(nullptr, true);
    ExpectNear(child->GetTransform().GetPosition(), { 11.0f, 0.0f, 0.0f });
    ExpectNear(child->GetTransform().GetLocalPosition(), { 11.0f, 0.0f, 0.0f });
    EXPECT_EQ(level.GetDefaultCollection()->GetRoots().size(), 2u);

    child->SetParent(parent, false);
    ExpectNear(child->GetTransform().GetPosition(), { 10.0f, 0.0f, -11.0f });
    EXPECT_TRUE(child->IsDescendantOf(parent));
    parent->SetParent(child);
    EXPECT_EQ(parent->GetParent(), nullptr);
}

TEST(LevelTests, LifecycleRunsInOrder)
{
    LifecycleLog().clear();
    RuntimeLevel level;
    auto* player = static_cast<TestPlayer*>(level.CreateObject(TestPlayer::StaticType()));
    auto* component = player->AddComponent<RecordingComponent>();
    EXPECT_TRUE(Contains(LifecycleLog(), "component:Initialize"));
    EXPECT_FALSE(Contains(LifecycleLog(), "component:Load"));

    level.BeginPlay();
    EXPECT_TRUE(Contains(LifecycleLog(), "Player:Load"));
    EXPECT_TRUE(Contains(LifecycleLog(), "component:Load"));
    EXPECT_TRUE(Contains(LifecycleLog(), "component:Enable"));
    EXPECT_FALSE(Contains(LifecycleLog(), "component:Start"));
    EXPECT_LT(IndexOf(LifecycleLog(), "component:Load"), IndexOf(LifecycleLog(), "component:Enable"));

    for (int i = 0; i < 60; ++i)
    {
        level.Update(1.0f / 60.0f);
    }
    EXPECT_TRUE(Contains(LifecycleLog(), "Player:Start"));
    EXPECT_TRUE(Contains(LifecycleLog(), "component:Start"));
    EXPECT_EQ(component->m_updates, 60);
    EXPECT_EQ(component->m_lateUpdates, 60);
    EXPECT_GE(component->m_fixedUpdates, 59);
    EXPECT_LE(component->m_fixedUpdates, 61);
    EXPECT_NEAR(player->m_elapsed, 1.0f, 1.0e-3f);

    player->SetActive(false);
    EXPECT_TRUE(Contains(LifecycleLog(), "component:Disable"));
    int updates = component->m_updates;
    level.Update(1.0f / 60.0f);
    EXPECT_EQ(component->m_updates, updates);
    player->SetActive(true);
    level.Update(1.0f / 60.0f);
    EXPECT_EQ(component->m_updates, updates + 1);

    LifecycleLog().clear();
    level.EndPlay();
    EXPECT_EQ(IndexOf(LifecycleLog(), "component:Disable"), 0);
    EXPECT_TRUE(Contains(LifecycleLog(), "Player:End"));
    EXPECT_TRUE(Contains(LifecycleLog(), "component:End"));
    EXPECT_LT(IndexOf(LifecycleLog(), "component:End"), IndexOf(LifecycleLog(), "component:Destroy"));
    EXPECT_LT(IndexOf(LifecycleLog(), "Player:End"), IndexOf(LifecycleLog(), "Player:Destroy"));
}

TEST(LevelTests, DestroyIsDeferredDuringPlay)
{
    LifecycleLog().clear();
    RuntimeLevel level;
    LevelObjectInstance* parent = level.CreateObject(LevelObjectInstance::StaticType());
    LevelObjectInstance* child = level.CreateObject(LevelObjectInstance::StaticType(), nullptr, parent);
    child->AddComponent<RecordingComponent>()->m_label = "child";
    level.BeginPlay();
    level.Update(0.016f);

    Core::Guid childId = child->GetId();
    parent->Destroy();
    EXPECT_TRUE(child->IsPendingDestroy());
    EXPECT_NE(level.FindObject(childId), nullptr);
    level.Update(0.016f);
    EXPECT_EQ(level.FindObject(childId), nullptr);
    EXPECT_EQ(level.GetObjectCount(), 0u);
    EXPECT_TRUE(Contains(LifecycleLog(), "child:Disable"));
    EXPECT_TRUE(Contains(LifecycleLog(), "child:Destroy"));
    EXPECT_FALSE(Contains(LifecycleLog(), "child:End"));
    level.EndPlay();
}

TEST(LevelTests, SpawnDuringPlayLoadsImmediatelyAndStartsNextFrame)
{
    LifecycleLog().clear();
    RuntimeLevel level;
    level.BeginPlay();
    LevelObjectInstance* spawned = level.CreateObject(LevelObjectInstance::StaticType());
    spawned->AddComponent<RecordingComponent>()->m_label = "spawned";
    EXPECT_TRUE(Contains(LifecycleLog(), "component:Load"));
    EXPECT_FALSE(Contains(LifecycleLog(), "spawned:Start"));
    level.Update(0.016f);
    EXPECT_TRUE(Contains(LifecycleLog(), "spawned:Start"));
    level.EndPlay();
}

TEST(LevelTests, CollectionRoundTripPreservesEverything)
{
    RuntimeLevel level;
    LevelCollection* collection = level.GetDefaultCollection();
    auto* player = static_cast<TestPlayer*>(level.CreateObject(TestPlayer::StaticType()));
    player->SetName("Hero");
    player->m_lives = 9;
    player->GetTransform().SetLocalPosition({ 1.0f, 2.0f, 3.0f });
    auto* component = player->AddComponent<RecordingComponent>();
    component->m_power = 4.5f;
    component->SetEnabled(false);
    LevelObjectInstance* child = level.CreateObject(LevelObjectInstance::StaticType(), nullptr, player);
    child->SetName("Weapon");
    child->SetActive(false);

    Serialization::JsonDocument document;
    LevelSerializer::WriteCollection(*collection, document, document.GetAllocator());
    std::string text = Serialization::WriteJson(document);
    EXPECT_NE(text.find("\"m_lives\": 9"), std::string::npos);

    RuntimeLevel loaded;
    LevelCollection* target = loaded.AddCollection("Copy");
    ASSERT_TRUE(LevelSerializer::ReadCollection(loaded, *target, document));
    EXPECT_EQ(target->GetName(), "Main");
    ASSERT_EQ(target->GetRoots().size(), 1u);
    auto* hero = Reflection::Cast<TestPlayer>(target->GetRoots()[0]);
    ASSERT_NE(hero, nullptr);
    EXPECT_EQ(hero->GetId(), player->GetId());
    EXPECT_EQ(hero->GetName(), "Hero");
    EXPECT_EQ(hero->m_lives, 9);
    ExpectNear(hero->GetTransform().GetPosition(), { 1.0f, 2.0f, 3.0f });
    auto* loadedComponent = hero->GetComponent<RecordingComponent>();
    ASSERT_NE(loadedComponent, nullptr);
    EXPECT_EQ(loadedComponent->m_power, 4.5f);
    EXPECT_FALSE(loadedComponent->IsEnabled());
    EXPECT_EQ(loadedComponent->GetLocalId(), component->GetLocalId());
    ASSERT_EQ(hero->GetChildren().size(), 1u);
    EXPECT_EQ(hero->GetChildren()[0]->GetName(), "Weapon");
    EXPECT_FALSE(hero->GetChildren()[0]->IsActiveSelf());
}

TEST(LevelTests, MissingTypesArePreserved)
{
    const char* json = R"({
        "name": "Main",
        "objects": [{
            "id": "0123456789abcdef0123456789abcdef",
            "name": "Ghost",
            "$type": "DeletedGameplayClass",
            "active": true,
            "properties": { "m_secret": 42 },
            "transform": { "m_localPosition": [1, 2, 3] },
            "components": [{ "id": "11111111111111111111111111111111", "$type": "DeletedComponent", "enabled": true, "properties": { "m_value": "kept" } }],
            "children": []
        }]
    })";
    Serialization::JsonDocument document;
    ASSERT_TRUE(Serialization::ParseJson(json, document));
    RuntimeLevel level;
    LevelCollection* collection = level.AddCollection("Main");
    LevelSerializer::ReadCollection(level, *collection, document);
    ASSERT_EQ(collection->GetRoots().size(), 1u);
    LevelObjectInstance* ghost = collection->GetRoots()[0];
    EXPECT_EQ(ghost->GetMissingTypeName(), "DeletedGameplayClass");
    ASSERT_EQ(ghost->GetComponents().size(), 1u);
    EXPECT_NE(Reflection::Cast<MissingComponent>(ghost->GetComponents()[0].get()), nullptr);

    Serialization::JsonDocument written;
    LevelSerializer::WriteCollection(*collection, written, written.GetAllocator());
    std::string text = Serialization::WriteJson(written);
    EXPECT_NE(text.find("DeletedGameplayClass"), std::string::npos);
    EXPECT_NE(text.find("\"m_secret\": 42"), std::string::npos);
    EXPECT_NE(text.find("DeletedComponent"), std::string::npos);
    EXPECT_NE(text.find("\"kept\""), std::string::npos);
}

TEST(LevelTests, CloneLevelCopiesForPlayMode)
{
    RuntimeLevel level;
    auto* player = static_cast<TestPlayer*>(level.CreateObject(TestPlayer::StaticType()));
    player->m_lives = 2;
    std::unique_ptr<RuntimeLevel> copy = LevelSerializer::CloneLevel(level);
    auto* copied = Reflection::Cast<TestPlayer>(copy->FindObject(player->GetId()));
    ASSERT_NE(copied, nullptr);
    EXPECT_NE(copied, player);
    copied->m_lives = 7;
    EXPECT_EQ(player->m_lives, 2);
}

TEST(AssetDatabaseTests, CreatesMetaAndCooksWithSourceHash)
{
    TempProject project;
    Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
    Serialization::JsonDocument content;
    content.SetObject();
    content.AddMember("$asset", "LevelCollection", content.GetAllocator());
    content.AddMember("name", "Streamed", content.GetAllocator());
    Core::Guid guid = database.CreateJsonAsset("Maps/Area.collection", content);
    ASSERT_TRUE(guid.IsValid());
    EXPECT_TRUE(std::filesystem::exists(project.Root() / "Assets" / "Maps" / "Area.collection.meta"));

    auto asset = database.Load<LevelCollectionAsset>(guid);
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(Serialization::GetString(asset->GetDocument(), "name"), "Streamed");
    EXPECT_TRUE(std::filesystem::exists(database.GetCookedDirectory() / (guid.ToString() + ".bin")));

    database.Close();
    database.Open(project.Root());
    EXPECT_EQ(database.FindRecordByPath("Maps/Area.collection")->guid, guid);

    content["name"].SetString("Changed");
    ASSERT_TRUE(database.SaveJsonAsset(guid, content));
    auto reloaded = database.Load<LevelCollectionAsset>(guid);
    EXPECT_EQ(Serialization::GetString(reloaded->GetDocument(), "name"), "Changed");
}

TEST(LevelObjectLinkTests, InstancesFollowAssetAndKeepOverrides)
{
    TempProject project;
    Assets::AssetDatabase& database = Assets::AssetDatabase::Get();

    RuntimeLevel authoring;
    auto* source = static_cast<TestPlayer*>(authoring.CreateObject(TestPlayer::StaticType()));
    source->SetName("Enemy");
    source->m_lives = 3;
    auto* sourceComponent = source->AddComponent<RecordingComponent>();
    sourceComponent->m_power = 1.0f;
    LevelObjectInstance* sourceChild = authoring.CreateObject(LevelObjectInstance::StaticType(), nullptr, source);
    sourceChild->SetName("Hat");

    Serialization::JsonDocument document;
    LevelSerializer::MakeLevelObjectDocument(*source, document);
    Core::Guid asset = database.CreateJsonAsset("Enemy.levelobject", document);
    ASSERT_TRUE(asset.IsValid());

    RuntimeLevel level;
    LevelObjectInstance* first = LevelSerializer::Instantiate(level, asset, nullptr, nullptr);
    LevelObjectInstance* second = LevelSerializer::Instantiate(level, asset, nullptr, nullptr);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first->GetId(), second->GetId());
    EXPECT_NE(first->GetChildren()[0]->GetId(), second->GetChildren()[0]->GetId());
    EXPECT_TRUE(first->GetLink().isRoot);
    EXPECT_EQ(first->GetChildren()[0]->GetLinkRoot(), first);

    auto* firstPlayer = Reflection::Cast<TestPlayer>(first);
    firstPlayer->m_lives = 99;
    first->GetTransform().SetLocalPosition({ 5.0f, 0.0f, 0.0f });
    EXPECT_TRUE(LevelSerializer::IsPropertyOverridden(*first, nullptr, "m_lives"));
    EXPECT_FALSE(LevelSerializer::IsPropertyOverridden(*first, nullptr, "m_moveSpeed"));
    EXPECT_FALSE(LevelSerializer::IsPropertyOverridden(*first, &first->GetTransform(), "m_localPosition"));
    auto overrides = LevelSerializer::ComputeOverrides(*first);
    ASSERT_EQ(overrides.size(), 1u);
    EXPECT_EQ(overrides[0].property, "m_lives");

    Serialization::JsonDocument collectionDocument;
    LevelSerializer::WriteCollection(*level.GetDefaultCollection(), collectionDocument, collectionDocument.GetAllocator());
    std::string text = Serialization::WriteJson(collectionDocument);
    EXPECT_NE(text.find("\"levelObject\""), std::string::npos);
    EXPECT_EQ(text.find("m_moveSpeed"), std::string::npos);

    RuntimeLevel reloaded;
    LevelCollection* reloadedCollection = reloaded.AddCollection("Main");
    LevelSerializer::ReadCollection(reloaded, *reloadedCollection, collectionDocument);
    auto* reloadedFirst = Reflection::Cast<TestPlayer>(reloaded.FindObject(first->GetId()));
    ASSERT_NE(reloadedFirst, nullptr);
    EXPECT_EQ(reloadedFirst->m_lives, 99);
    ExpectNear(reloadedFirst->GetTransform().GetPosition(), { 5.0f, 0.0f, 0.0f });
    EXPECT_EQ(reloadedFirst->GetChildren()[0]->GetId(), first->GetChildren()[0]->GetId());

    Core::Guid firstId = first->GetId();
    Core::Guid secondId = second->GetId();
    auto* secondComponent = second->GetComponent<RecordingComponent>();
    secondComponent->m_power = 8.0f;
    ASSERT_TRUE(LevelSerializer::ApplyPropertyToAsset(*second, secondComponent, "m_power"));
    LevelSerializer::RefreshLinkedInstances(level, asset);

    auto* refreshedFirst = Reflection::Cast<TestPlayer>(level.FindObject(firstId));
    ASSERT_NE(refreshedFirst, nullptr);
    EXPECT_EQ(refreshedFirst->m_lives, 99);
    EXPECT_EQ(refreshedFirst->GetComponent<RecordingComponent>()->m_power, 8.0f);
    LevelObjectInstance* refreshedSecond = level.FindObject(secondId);
    EXPECT_FALSE(LevelSerializer::IsPropertyOverridden(*refreshedSecond, refreshedSecond->GetComponent<RecordingComponent>(), "m_power"));

    LevelSerializer::RevertProperty(*refreshedFirst, nullptr, "m_lives");
    EXPECT_EQ(refreshedFirst->m_lives, 3);
    EXPECT_TRUE(LevelSerializer::ComputeOverrides(*refreshedFirst).empty());
}

TEST(LevelObjectLinkTests, TemplatesCannotBeInstantiated)
{
    TempProject project;
    RuntimeLevel authoring;
    LevelObjectInstance* source = authoring.CreateObject(LevelObjectInstance::StaticType());
    Serialization::JsonDocument document;
    LevelSerializer::MakeLevelObjectDocument(*source, document);
    Core::Guid templateAsset = Assets::AssetDatabase::Get().CreateJsonAsset("Base.template", document);
    ASSERT_TRUE(templateAsset.IsValid());
    RuntimeLevel level;
    EXPECT_EQ(LevelSerializer::Instantiate(level, templateAsset, nullptr, nullptr), nullptr);
}
