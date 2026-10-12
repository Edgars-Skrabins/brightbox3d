#include "Core/FileSystem.h"
#include "Core/Guid.h"
#include "Core/Hash.h"
#include "Core/Process.h"
#include "Core/StringUtils.h"

#include <gtest/gtest.h>

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
    auto path = std::filesystem::temp_directory_path() / "BrightBox3DTests" / "file.txt";
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
#endif
