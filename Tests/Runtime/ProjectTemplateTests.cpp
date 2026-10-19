#include "Core/TemplateFile.h"
#include "Runtime/ProjectTemplates.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace BrightBox3D;

TEST(ProjectTemplateTests, PlaceholdersAreReplacedEverywhere)
{
    const Core::TemplateValue values[] = { { "NAME", "Arena" }, { "EMPTY", "" } };
    EXPECT_EQ(Core::RenderTemplateText("@NAME@ and @NAME@@EMPTY@ @OTHER@", values), "Arena and Arena @OTHER@");
}

TEST(ProjectTemplateTests, GeneratedFilesMatchThePreviousOutput)
{
    std::optional<std::string> gitIgnore = Runtime::ProjectTemplates::GitIgnore();
    ASSERT_TRUE(gitIgnore);
    EXPECT_EQ(*gitIgnore, "Cache/\nBuild/\nBinaries/\nCMakeLists.txt\n.DS_Store\n");

    std::optional<std::string> cmakeLists = Runtime::ProjectTemplates::GameCMakeLists("ArenaDemo", "/Engine/bin/BrightBox3DSDK.cmake");
    ASSERT_TRUE(cmakeLists);
    EXPECT_EQ(*cmakeLists,
        "cmake_minimum_required(VERSION 3.24)\n"
        "project(ArenaDemo LANGUAGES C CXX)\n\n"
        "set(BRIGHT_SDK \"/Engine/bin/BrightBox3DSDK.cmake\" CACHE FILEPATH \"\")\n"
        "include(${BRIGHT_SDK})\n"
        "include(${BRIGHT_ENGINE_SOURCE_DIR}/cmake/BrightGame.cmake)\n\n"
        "bright_add_game_module(ArenaDemo ${CMAKE_CURRENT_SOURCE_DIR}/Scripts ${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Staging)\n");
    EXPECT_NE(cmakeLists->find(Runtime::ProjectTemplates::GeneratedMarker), std::string::npos);
}
