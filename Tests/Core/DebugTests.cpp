#include "Core/Debug.h"
#include "Core/Log.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace BrightBox3D;

namespace
{
    std::vector<Core::LogEntry> Entries()
    {
        std::vector<Core::LogEntry> entries;
        Core::Log::ForEach([&](const Core::LogEntry& entry) { entries.push_back(entry); });
        return entries;
    }
}

TEST(DebugTests, GameLogsCaptureCallerFileLineAndLevel)
{
    Core::Log::SetEcho(false);
    Core::Log::Clear();
    int line = __LINE__ + 1;
    Debug::Log("started");
    Debug::LogWarning("careful");
    Debug::LogError("failed");
    std::vector<Core::LogEntry> entries = Entries();
    Core::Log::Clear();
    Core::Log::SetEcho(true);

    ASSERT_EQ(entries.size(), 3u);
    const Core::LogLevel levels[] = { Core::LogLevel::Info, Core::LogLevel::Warning, Core::LogLevel::Error };
    const char* messages[] = { "started", "careful", "failed" };
    for (size_t i = 0; i < entries.size(); ++i)
    {
        EXPECT_EQ(entries[i].source, Core::LogSource::Game);
        EXPECT_EQ(entries[i].level, levels[i]);
        EXPECT_EQ(entries[i].message, messages[i]);
        EXPECT_EQ(entries[i].category, "Game");
        EXPECT_NE(entries[i].file.find("DebugTests.cpp"), std::string::npos) << entries[i].file;
        EXPECT_EQ(entries[i].line, line + static_cast<int>(i));
    }
}
