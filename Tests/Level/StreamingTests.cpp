#include "Assets/AssetDatabase.h"
#include "Common/TestUtilities.h"
#include "Level/Level.h"
#include "Level/LevelSerializer.h"
#include "Level/LevelStreaming.h"
#include "Level/LevelTestTypes.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <thread>

using namespace BrightBox3D;
using BrightBox3D::Level::LevelObject;

TEST(StreamingTests, LoadsAndUnloadsCollectionsDuringPlay)
{
    std::filesystem::path root = Tests::MakeTempDirectory("Streaming");
    std::filesystem::create_directories(root / "Assets");
    Assets::AssetDatabase::Get().Open(root);

    Level::Level authoring;
    Level::LevelCollection* source = authoring.AddCollection("Dungeon");
    for (int i = 0; i < 25; ++i)
    {
        LevelObject* object = authoring.CreateObject(Tests::TestPlayer::StaticType(), source);
        object->GetTransform().SetLocalPosition({ static_cast<float>(i), 0.0f, 0.0f });
    }
    Serialization::JsonDocument document;
    Level::LevelSerializer::WriteCollection(*source, document, document.GetAllocator());
    Core::Guid dungeon = Assets::AssetDatabase::Get().CreateJsonAsset("Dungeon.collection", document);
    ASSERT_TRUE(dungeon.IsValid());

    Tests::LifecycleLog().clear();
    Level::Level level;
    level.BeginPlay();
    Level::LevelCollection* loaded = nullptr;
    int callbacks = 0;
    Level::LevelStreaming::LoadAsync(level, dungeon, [&](Level::LevelCollection* collection)
    {
        loaded = collection;
        ++callbacks;
    });
    EXPECT_TRUE(Level::LevelStreaming::Get(&level)->IsLoading(dungeon));
    auto start = std::chrono::steady_clock::now();
    while (!loaded && std::chrono::steady_clock::now() - start < std::chrono::seconds(10))
    {
        level.Update(1.0f / 60.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(callbacks, 1);
    EXPECT_EQ(loaded->GetRoots().size(), 25u);
    EXPECT_EQ(loaded->GetName(), "Dungeon");
    EXPECT_TRUE(std::count(Tests::LifecycleLog().begin(), Tests::LifecycleLog().end(), "Player:Load") == 25);
    level.Update(1.0f / 60.0f);
    EXPECT_TRUE(std::count(Tests::LifecycleLog().begin(), Tests::LifecycleLog().end(), "Player:Start") == 25);

    EXPECT_TRUE(Level::LevelStreaming::Unload(level, dungeon));
    level.Update(1.0f / 60.0f);
    level.Update(1.0f / 60.0f);
    EXPECT_EQ(level.FindCollection(dungeon), nullptr);
    EXPECT_EQ(level.FindObjectsOfType<Tests::TestPlayer>().size(), 0u);
    EXPECT_TRUE(std::count(Tests::LifecycleLog().begin(), Tests::LifecycleLog().end(), "Player:Destroy") == 25);
    level.EndPlay();

    Assets::AssetDatabase::Get().Close();
    std::error_code error;
    std::filesystem::remove_all(root, error);
}
