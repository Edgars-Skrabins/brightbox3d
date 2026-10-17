#include "Assets/AssetDatabase.h"
#include "Common/TestUtilities.h"
#include "Level/Level.h"
#include "Level/LevelCollection.h"
#include "Level/LevelSerializer.h"
#include "Runtime/Project.h"
#include "Serialization/Json.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

using namespace BrightBox3D;

TEST(LevelFileFormatTests, SampleCollectionsRewriteToTheSameJson)
{
    std::filesystem::path root = Tests::MakeTempDirectory("LevelFormat");
    std::filesystem::copy(std::filesystem::path(BRIGHT_SAMPLES_DIR) / "ArenaDemo" / "Assets", root / "Assets", std::filesystem::copy_options::recursive);
    std::filesystem::copy_file(std::filesystem::path(BRIGHT_SAMPLES_DIR) / "ArenaDemo" / Runtime::Project::FileName, root / Runtime::Project::FileName);
    Runtime::Project project;
    ASSERT_TRUE(Tests::Succeeded(project.Open(root)));
    std::string error;
    ASSERT_TRUE(Assets::AssetDatabase::Get().Open(root));

    std::unique_ptr<Level::Level> level = Level::LevelSerializer::LoadLevel(project.GetStartupLevel(), &error);
    ASSERT_NE(level, nullptr) << error;
    ASSERT_FALSE(level->GetCollections().empty());
    for (const auto& collection : level->GetCollections())
    {
        const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(collection->GetAsset());
        ASSERT_NE(record, nullptr);
        Serialization::JsonDocument original;
        ASSERT_TRUE(Serialization::LoadJsonFile(Assets::AssetDatabase::Get().GetAssetsDirectory() / record->path, original));
        Serialization::JsonDocument rewritten;
        Level::LevelSerializer::WriteCollection(*collection, rewritten, rewritten.GetAllocator());
        EXPECT_TRUE(rewritten == original) << record->path;
    }
    level.reset();
    Assets::AssetDatabase::Get().Close();
    std::filesystem::remove_all(root);
}
