#include "Core/FileSystem.h"
#include "Core/Guid.h"
#include "RecentProjects.h"
#include "Runtime/Project.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace BrightBox3D;

TEST(RecentProjectsTests, LegacyListLoadsAndNewFormatRoundTrips)
{
    std::filesystem::path directory = std::filesystem::temp_directory_path() / ("BrightTests_Recent_" + Core::Guid::Generate().ToString());
    std::filesystem::create_directories(directory);
    std::filesystem::path sample = std::filesystem::path(BRIGHT_SAMPLES_DIR) / "ArenaDemo";
    std::filesystem::path missing = directory / "Missing";
    Core::FileSystem::WriteText(directory / "Legacy.json", "[\"" + sample.generic_string() + "\", \"" + missing.generic_string() + "\"]");

    Editor::RecentProjects recent;
    recent.Load(directory / "Legacy.json");
    ASSERT_EQ(recent.GetProjects().size(), 1u);
    EXPECT_EQ(recent.GetProjects()[0], sample.generic_string());

    for (size_t index = 0; index < Editor::RecentProjects::MaxCount + 3; ++index)
    {
        recent.Add(sample);
    }
    EXPECT_EQ(recent.GetProjects().size(), 1u);
    recent.Save(directory / Editor::RecentProjects::FileName);

    Editor::RecentProjects loaded;
    loaded.Load(directory / Editor::RecentProjects::FileName);
    EXPECT_EQ(loaded.GetProjects(), recent.GetProjects());
    std::filesystem::remove_all(directory);
}
