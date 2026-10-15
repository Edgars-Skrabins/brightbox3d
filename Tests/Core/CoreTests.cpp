#include "Common/TestUtilities.h"
#include "Core/FileSystem.h"
#include "Core/Guid.h"
#include "Core/Hash.h"
#include "Core/Process.h"
#include "Core/StringUtils.h"
#include "Platform/Shell.h"
#include "Platform/ShellLaunch.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

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

TEST(GuidTests, ParseAcceptsEitherCaseAndRejectsMalformedText)
{
    Guid guid = Guid::Parse("0123456789abcdef0123456789ABCDEF");
    EXPECT_TRUE(guid.IsValid());
    EXPECT_EQ(guid.high, 0x0123456789ABCDEFull);
    EXPECT_EQ(guid.low, 0x0123456789ABCDEFull);
    EXPECT_EQ(guid.ToString(), "0123456789abcdef0123456789abcdef");
    EXPECT_FALSE(Guid::Parse("0123456789abcdef0123456789abcde").IsValid());
    EXPECT_FALSE(Guid::Parse("0123456789abcdef0123456789abcdef0").IsValid());
    EXPECT_FALSE(Guid::Parse("0123456789abcdef0123456789abcdeg").IsValid());
    EXPECT_FALSE(Guid::Parse("").IsValid());
    EXPECT_FALSE(Guid{}.IsValid());
    EXPECT_EQ(Guid{}.ToString(), std::string(32, '0'));
}

TEST(GuidTests, OrderingAndHashingDeduplicateInContainers)
{
    Guid first = Guid::Parse("00000000000000010000000000000000");
    Guid second = Guid::Parse("00000000000000010000000000000001");
    Guid third = Guid::Parse("00000000000000020000000000000000");
    EXPECT_TRUE(first < second);
    EXPECT_TRUE(second < third);
    EXPECT_FALSE(third < first);

    std::set<Guid> ordered = { third, first, second, first };
    EXPECT_EQ(ordered.size(), 3u);
    EXPECT_EQ(*ordered.begin(), first);
    std::unordered_set<Guid> hashed = { third, first, second, third };
    EXPECT_EQ(hashed.size(), 3u);
    EXPECT_TRUE(hashed.count(second));
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

TEST(StringUtilsTests, CaseAndAffixChecks)
{
    EXPECT_EQ(ToLower("MiXeD.PNG"), "mixed.png");
    EXPECT_TRUE(StartsWith("Assets/Main.css", "Assets/"));
    EXPECT_TRUE(StartsWith("Assets", ""));
    EXPECT_FALSE(StartsWith("As", "Assets"));
    EXPECT_TRUE(EndsWith("Hud.screen.xml", ".screen.xml"));
    EXPECT_FALSE(EndsWith("xml", "screen.xml"));
    EXPECT_FALSE(EndsWith("Main.CSS", ".css"));
}

TEST(StringUtilsTests, ReplaceAllHandlesGrowthAndEmptyPatterns)
{
    EXPECT_EQ(ReplaceAll("a.b.c", ".", "/"), "a/b/c");
    EXPECT_EQ(ReplaceAll("aaa", "a", "aa"), "aaaaaa");
    EXPECT_EQ(ReplaceAll("{Name} and {Name}", "{Name}", ""), " and ");
    EXPECT_EQ(ReplaceAll("unchanged", "", "x"), "unchanged");
    EXPECT_EQ(ReplaceAll("unchanged", "missing", "x"), "unchanged");
}

TEST(StringUtilsTests, SplitKeepsLeadingAndTrailingEmptyParts)
{
    std::vector<std::string> expected = { "", "a", "", "b", "" };
    EXPECT_EQ(Split(",a,,b,", ','), expected);
    EXPECT_EQ(Split("", ','), std::vector<std::string>{ "" });
    EXPECT_EQ(Trim(" \t\r\n "), "");
}

TEST(StringUtilsTests, CountsFormatPlaceholdersButNotEscapedBraces)
{
    EXPECT_EQ(CountFormatPlaceholders("No placeholders"), 0);
    EXPECT_EQ(CountFormatPlaceholders("{} of {}"), 2);
    EXPECT_EQ(CountFormatPlaceholders("{{literal}} {}"), 1);
    EXPECT_EQ(CountFormatPlaceholders("{0} {1:.2f}"), 2);
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

TEST(ShellTests, ShutdownStopsLaunchesThatAreStillRunning)
{
    auto directory = BrightBox3D::Tests::MakeTempDirectory("ShellLaunch");
    auto marker = directory / "started";
    BrightBox3D::Platform::Shell::Launch({ "sh", "-c", "touch \"" + marker.string() + "\"; sleep 30" });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline && !std::filesystem::exists(marker))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(std::filesystem::exists(marker));

    auto start = std::chrono::steady_clock::now();
    BrightBox3D::Platform::Shell::Shutdown();
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(3));
    std::filesystem::remove_all(directory);
}
#endif
