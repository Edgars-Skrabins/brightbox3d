#include "Common/TestUtilities.h"
#include "Core/FileSystem.h"
#include "Core/StringUtils.h"
#include "EditorText.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <set>
#include <string>

using namespace BrightBox3D;
using Editor::EditorText;

namespace
{
    std::filesystem::path WriteOverrides(const std::string& name, const std::string& json)
    {
        std::filesystem::path path = Tests::MakeTempDirectory("EditorText") / name;
        Core::FileSystem::WriteText(path, json);
        return path;
    }

    class EditorTextTests : public ::testing::Test
    {
    protected:
        void TearDown() override
        {
            Editor::EditorTextTable::Get().Reset();
        }
    };
}

TEST_F(EditorTextTests, EveryEntryHasUniqueKeyAndEnglishText)
{
    std::set<std::string> keys;
    for (size_t index = 0; index < static_cast<size_t>(EditorText::Count); ++index)
    {
        const Editor::EditorTextInfo& info = Editor::GetTextInfo(static_cast<EditorText>(index));
        EXPECT_TRUE(keys.insert(info.key).second) << info.key;
        EXPECT_FALSE(std::string(info.english).empty()) << info.key;
    }
}

TEST_F(EditorTextTests, PlaceholderCountMatchesArgumentCount)
{
    for (size_t index = 0; index < static_cast<size_t>(EditorText::Count); ++index)
    {
        const Editor::EditorTextInfo& info = Editor::GetTextInfo(static_cast<EditorText>(index));
        EXPECT_EQ(Core::CountFormatPlaceholders(info.english), info.argumentCount) << info.key;
    }
}

TEST_F(EditorTextTests, FormatsArguments)
{
    EXPECT_EQ(Editor::Text(EditorText::OpenedProject, std::string("Arena")), "Opened project 'Arena'");
    EXPECT_EQ(Editor::Text(EditorText::Reloaded, 1.5), "Reloaded (1.5s)");
}

TEST_F(EditorTextTests, OverrideFileReplacesText)
{
    std::filesystem::path path = WriteOverrides("Valid.json", R"({ "Open": "Ouvrir", "OpenedProject": "Projet '{}' ouvert" })");
    std::string error;
    ASSERT_TRUE(Editor::EditorTextTable::Get().LoadOverrides(path, error)) << error;
    EXPECT_STREQ(Editor::Text(EditorText::Open), "Ouvrir");
    EXPECT_EQ(Editor::Text(EditorText::OpenedProject, std::string("Arena")), "Projet 'Arena' ouvert");
    EXPECT_STREQ(Editor::Text(EditorText::Cancel), "Cancel");
}

TEST_F(EditorTextTests, OverrideWithWrongPlaceholderCountIsIgnored)
{
    std::filesystem::path path = WriteOverrides("Mismatch.json", R"({ "OpenedProject": "Projet ouvert" })");
    std::string error;
    ASSERT_TRUE(Editor::EditorTextTable::Get().LoadOverrides(path, error)) << error;
    EXPECT_EQ(Editor::Text(EditorText::OpenedProject, std::string("Arena")), "Opened project 'Arena'");
}

TEST_F(EditorTextTests, InvalidOverrideFileFails)
{
    std::filesystem::path path = WriteOverrides("Invalid.json", "[1, 2]");
    std::string error;
    EXPECT_FALSE(Editor::EditorTextTable::Get().LoadOverrides(path, error));
    EXPECT_FALSE(error.empty());
}
