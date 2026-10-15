#include "Core/Log.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace BrightBox3D::Core;

namespace
{
    class LogTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            Log::SetEcho(false);
            Log::Clear();
        }

        void TearDown() override
        {
            Log::Clear();
            Log::SetEcho(true);
        }
    };
}

TEST_F(LogTests, ShortcutsAndDefaultWriteAreTaggedEngine)
{
    Log::Info("info");
    Log::Warning("warning");
    Log::Error("error");
    Log::Write(LogLevel::Info, "default");
    std::vector<LogSource> sources;
    Log::ForEach([&](const LogEntry& entry) { sources.push_back(entry.source); });
    ASSERT_EQ(sources.size(), 4u);
    for (LogSource source : sources)
    {
        EXPECT_EQ(source, LogSource::Engine);
    }
}

TEST_F(LogTests, GameEntriesKeepCategoryFileAndLine)
{
    Log::Write(LogLevel::Warning, "careful", LogSource::Game, LogCategory::Game, "Player.cpp", 42);
    LogEntry found;
    Log::ForEach([&](const LogEntry& entry) { found = entry; });
    EXPECT_EQ(found.source, LogSource::Game);
    EXPECT_EQ(found.level, LogLevel::Warning);
    EXPECT_EQ(found.message, "careful");
    EXPECT_EQ(found.category, LogCategory::Game);
    EXPECT_EQ(found.file, "Player.cpp");
    EXPECT_EQ(found.line, 42);
}

TEST_F(LogTests, CountsAreKeptPerSourceAndLevel)
{
    Log::Write(LogLevel::Info, "a", LogSource::Game);
    Log::Write(LogLevel::Info, "b", LogSource::Game);
    Log::Write(LogLevel::Error, "c", LogSource::Game);
    Log::Write(LogLevel::Warning, "d", LogSource::Engine);
    Log::Write(LogLevel::Info, "e", LogSource::Engine);

    EXPECT_EQ(Log::Count(LogLevel::Info, LogSource::Game), 2);
    EXPECT_EQ(Log::Count(LogLevel::Warning, LogSource::Game), 0);
    EXPECT_EQ(Log::Count(LogLevel::Error, LogSource::Game), 1);
    EXPECT_EQ(Log::Count(LogLevel::Warning, LogSource::Engine), 1);
    EXPECT_EQ(Log::Count(LogSource::Game), 3);
    EXPECT_EQ(Log::Count(LogSource::Engine), 2);
    EXPECT_TRUE(Log::HasErrors(LogSource::Game));
    EXPECT_FALSE(Log::HasErrors(LogSource::Engine));

    Log::Write(LogLevel::Error, "f", LogSource::Engine);
    EXPECT_TRUE(Log::HasErrors(LogSource::Engine));
}

TEST_F(LogTests, ClearResetsEveryCount)
{
    Log::Write(LogLevel::Error, "a", LogSource::Game);
    Log::Write(LogLevel::Warning, "b", LogSource::Engine);
    uint64_t revision = Log::Revision();
    Log::Clear();

    EXPECT_GT(Log::Revision(), revision);
    EXPECT_EQ(Log::Count(LogSource::Game), 0);
    EXPECT_EQ(Log::Count(LogSource::Engine), 0);
    EXPECT_FALSE(Log::HasErrors(LogSource::Game));
    int entries = 0;
    Log::ForEach([&](const LogEntry&) { ++entries; });
    EXPECT_EQ(entries, 0);
}
