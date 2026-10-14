#include "Common/TestUtilities.h"
#include "Core/FileSystem.h"
#include "Core/Guid.h"
#include "Core/Hash.h"
#include "Core/Process.h"
#include "Core/StringUtils.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <thread>

#if !defined(_WIN32)
#include <csignal>
#endif

using namespace BrightBox3D::Core;

TEST(GuidTests, RoundTripsThroughString)
{
    Guid guid = Guid::Generate();
    EXPECT_TRUE(guid.IsValid());
    EXPECT_EQ(Guid::Parse(guid.ToString()), guid);
    EXPECT_FALSE(Guid::Parse("not-a-guid").IsValid());
}

TEST(GuidTests, GeneratesUniqueValues)
{
    EXPECT_NE(Guid::Generate(), Guid::Generate());
}

TEST(StringUtilsTests, PrettifiesMemberNames)
{
    EXPECT_EQ(PrettifyName("m_moveSpeed"), "Move Speed");
    EXPECT_EQ(PrettifyName("m_isActive"), "Is Active");
    EXPECT_EQ(PrettifyName("m_maxHP"), "Max HP");
    EXPECT_EQ(PrettifyName("m_layer2Mask"), "Layer 2 Mask");
    EXPECT_EQ(PrettifyName("health"), "Health");
}

TEST(StringUtilsTests, SplitAndTrim)
{
    auto parts = Split("a,b,,c", ',');
    ASSERT_EQ(parts.size(), 4u);
    EXPECT_EQ(parts[2], "");
    EXPECT_EQ(Trim("  hello \n"), "hello");
}

TEST(HashTests, IsStable)
{
    EXPECT_EQ(HashString("abc"), HashString("abc"));
    EXPECT_NE(HashString("abc"), HashString("abd"));
}

TEST(FileSystemTests, WritesAndReadsText)
{
    auto path = BrightBox3D::Tests::MakeTempDirectory("FileSystem") / "file.txt";
    ASSERT_TRUE(FileSystem::WriteText(path, "content"));
    auto text = FileSystem::ReadText(path);
    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, "content");
    std::filesystem::remove_all(path.parent_path());
}

#if !defined(_WIN32)
TEST(ProcessTests, CapturesOutputAndExitCode)
{
    ProcessResult result = Process::Run({ "sh", "-c", "echo hello; exit 3" });
    EXPECT_TRUE(result.started);
    EXPECT_EQ(result.exitCode, 3);
    EXPECT_EQ(result.output, "hello\n");
}

TEST(ProcessTests, TerminateKillsTheWholeProcessTree)
{
    auto directory = BrightBox3D::Tests::MakeTempDirectory("Terminate");
    auto pidFile = directory / "child.pid";

    AsyncProcess process;
    process.Start({ { "sh", "-c", "sleep 30 & echo $! > \"" + pidFile.string() + "\"; wait" } });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::optional<std::string> pidText;
    while (std::chrono::steady_clock::now() < deadline && !(pidText = FileSystem::ReadText(pidFile)).has_value())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(pidText.has_value());
    pid_t child = static_cast<pid_t>(std::stoi(Trim(*pidText)));

    auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(process.Terminate(std::chrono::seconds(2)));
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(3));
    EXPECT_FALSE(process.IsRunning());
    ProcessResult result = process.TakeResult();
    EXPECT_TRUE(result.terminated);

    bool childGone = false;
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(3) && !childGone)
    {
        childGone = kill(child, 0) != 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(childGone);
    std::filesystem::remove_all(directory);
}
#endif
