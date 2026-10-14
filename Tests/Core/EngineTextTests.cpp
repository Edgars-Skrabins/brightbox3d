#include "Common/TestUtilities.h"
#include "Core/EngineText.h"
#include "Core/FileSystem.h"
#include "Core/StringUtils.h"
#include "Serialization/EngineTextOverrides.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

using namespace BrightBox3D;
using Core::EngineText;

namespace
{
    class EngineTextTests : public ::testing::Test
    {
    protected:
        void TearDown() override
        {
            Core::EngineTextTable::Get().Reset();
        }

        std::filesystem::path WriteOverrides(const std::string& json)
        {
            std::filesystem::path path = Tests::MakeTempDirectory("EngineText") / "Overrides.json";
            Core::FileSystem::WriteText(path, json);
            return path;
        }
    };
}

TEST_F(EngineTextTests, EveryEntryHasUniqueKeyAndMatchingPlaceholders)
{
    std::set<std::string> keys;
    for (size_t index = 0; index < static_cast<size_t>(EngineText::Count); ++index)
    {
        const Core::EngineTextInfo& info = Core::GetTextInfo(static_cast<EngineText>(index));
        EXPECT_TRUE(keys.insert(info.key).second) << info.key;
        EXPECT_FALSE(std::string(info.english).empty()) << info.key;
        EXPECT_EQ(Core::CountFormatPlaceholders(info.english), info.argumentCount) << info.key;
        EXPECT_EQ(Core::FindEngineText(info.key), static_cast<EngineText>(index));
    }
}

TEST_F(EngineTextTests, FormatsArguments)
{
    EXPECT_EQ(Core::Text(EngineText::GameModuleLoaded, std::string("Arena"), 4), "Loaded game module 'Arena' (4 reflected types)");
    EXPECT_EQ(Core::Text(EngineText::CssMissingBrace), std::string("missing '}'"));
}

TEST_F(EngineTextTests, OverrideFileReplacesValidEntriesAndRejectsTheRest)
{
    std::filesystem::path path = WriteOverrides(R"json({ "CannotReadFile": "Datei unlesbar", "GameModuleLoaded": "Modul '{}' geladen ({})", "ProjectInvalid": "{} broken", "NoSuchText": "x", "LogLevelInfo": 3 })json");
    std::vector<std::string> rejected;
    std::string error;
    ASSERT_TRUE(Serialization::LoadEngineTextOverrides(path, rejected, error)) << error;
    EXPECT_STREQ(Core::Text(EngineText::CannotReadFile), "Datei unlesbar");
    EXPECT_EQ(Core::Text(EngineText::GameModuleLoaded, std::string("Arena"), 4), "Modul 'Arena' geladen (4)");
    EXPECT_STREQ(Core::Text(EngineText::ProjectInvalid), "Invalid project file");
    EXPECT_EQ(rejected, (std::vector<std::string>{ "ProjectInvalid", "NoSuchText", "LogLevelInfo" }));
    std::filesystem::remove(path);
}

TEST_F(EngineTextTests, InvalidOverrideFileFails)
{
    std::filesystem::path path = WriteOverrides("[1, 2]");
    std::vector<std::string> rejected;
    std::string error;
    EXPECT_FALSE(Serialization::LoadEngineTextOverrides(path, rejected, error));
    EXPECT_FALSE(error.empty());
    std::filesystem::remove(path);
}
