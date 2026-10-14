#include "Common/TestUtilities.h"
#include "Core/FileSystem.h"
#include "Core/Guid.h"
#include "Runtime/GameManifest.h"
#include "Runtime/Project.h"
#include "Serialization/Json.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace BrightBox3D;

namespace
{
}

TEST(ProjectFileTests, SampleProjectWithNestedSettingsLoads)
{
    std::filesystem::path directory = Tests::MakeTempDirectory("Project");
    std::filesystem::copy_file(std::filesystem::path(BRIGHT_SAMPLES_DIR) / "ArenaDemo" / Runtime::Project::FileName, directory / Runtime::Project::FileName);
    Runtime::Project project;
    std::string error;
    ASSERT_TRUE(project.Open(directory, error)) << error;
    EXPECT_EQ(project.GetName(), "ArenaDemo");
    EXPECT_EQ(project.GetGameModuleName(), "ArenaDemo");
    EXPECT_EQ(project.GetStartupLevel(), Core::Guid::Parse("8660f0967eb8f80adf88b2b3281c4293"));
    EXPECT_NEAR(project.GetFixedTimeStep(), 1.0f / 60.0f, 1.0e-6f);
    EXPECT_TRUE(project.GetAutoHotReload());

    project.SetFixedTimeStep(1.0f / 30.0f);
    project.SetAutoHotReload(false);
    ASSERT_TRUE(project.Save());
    Serialization::JsonDocument saved;
    ASSERT_TRUE(Serialization::LoadJsonFile(directory / Runtime::Project::FileName, saved));
    EXPECT_EQ(Serialization::FindMember(saved, "settings"), nullptr);
    EXPECT_EQ(Serialization::GetString(saved, "engineVersion"), "0.1.0");

    Runtime::Project reopened;
    ASSERT_TRUE(reopened.Open(directory, error)) << error;
    EXPECT_EQ(reopened.GetName(), "ArenaDemo");
    EXPECT_NEAR(reopened.GetFixedTimeStep(), 1.0f / 30.0f, 1.0e-6f);
    EXPECT_FALSE(reopened.GetAutoHotReload());
    EXPECT_EQ(reopened.GetStartupLevel(), project.GetStartupLevel());
    std::filesystem::remove_all(directory);
}

TEST(ProjectFileTests, CreatedProjectUsesSanitizedModuleName)
{
    std::filesystem::path directory = Tests::MakeTempDirectory("Create");
    std::string error;
    ASSERT_TRUE(Runtime::Project::Create(directory, "My Game 2", error)) << error;
    Runtime::Project project;
    ASSERT_TRUE(project.Open(directory, error)) << error;
    EXPECT_EQ(project.GetName(), "My Game 2");
    EXPECT_EQ(project.GetGameModuleName(), "MyGame2");
    EXPECT_FALSE(project.GetStartupLevel().IsValid());
    std::filesystem::remove_all(directory);
}

TEST(ProjectFileTests, GameManifestRoundTripsAndReadsOldFiles)
{
    std::filesystem::path directory = Tests::MakeTempDirectory("Manifest");
    Core::FileSystem::WriteText(directory / "Old.json", R"({ "name": "Arena", "gameModule": "ArenaDemo", "startupLevel": "8660f0967eb8f80adf88b2b3281c4293", "fixedTimeStep": 0.02 })");
    Runtime::GameManifest manifest;
    std::string error;
    ASSERT_TRUE(manifest.Load(directory / "Old.json", error)) << error;
    EXPECT_EQ(manifest.name, "Arena");
    EXPECT_EQ(manifest.gameModule, "ArenaDemo");
    EXPECT_NEAR(manifest.fixedTimeStep, 0.02f, 1.0e-6f);

    ASSERT_TRUE(manifest.Save(directory / Runtime::GameManifest::FileName));
    Runtime::GameManifest loaded;
    ASSERT_TRUE(loaded.Load(directory / Runtime::GameManifest::FileName, error)) << error;
    EXPECT_EQ(loaded.name, manifest.name);
    EXPECT_EQ(loaded.gameModule, manifest.gameModule);
    EXPECT_EQ(loaded.startupLevel, manifest.startupLevel);
    EXPECT_FLOAT_EQ(loaded.fixedTimeStep, manifest.fixedTimeStep);
    std::filesystem::remove_all(directory);
}
