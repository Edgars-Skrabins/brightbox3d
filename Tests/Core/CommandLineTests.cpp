#include "Core/CommandLine.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace BrightBox3D;

namespace
{
    struct TestOptions
    {
        std::filesystem::path input;
        std::filesystem::path output;
        int count = -1;
        std::string name;
        bool verbose = false;
    };

    const Core::CommandLineOption<TestOptions> TestCommandLine[] = {
        { nullptr, &TestOptions::input },
        { "--output", &TestOptions::output },
        { "--count", &TestOptions::count },
        { "--name", &TestOptions::name },
        { "--verbose", &TestOptions::verbose },
    };

    Core::Result<TestOptions> Parse(std::vector<std::string> arguments)
    {
        arguments.insert(arguments.begin(), "Tool");
        std::vector<char*> argv;
        for (std::string& argument : arguments)
        {
            argv.push_back(argument.data());
        }
        return Core::ParseCommandLine<TestOptions>(static_cast<int>(argv.size()), argv.data(), TestCommandLine);
    }
}

TEST(CommandLineTests, ParsesEveryValueType)
{
    Core::Result<TestOptions> options = Parse({ "Project", "--output", "out/shot.png", "--count", "42", "--name", "Crate", "--verbose" });
    ASSERT_TRUE(options) << options.GetError().GetText();
    EXPECT_EQ(options->input, std::filesystem::path("Project"));
    EXPECT_EQ(options->output, std::filesystem::path("out/shot.png"));
    EXPECT_EQ(options->count, 42);
    EXPECT_EQ(options->name, "Crate");
    EXPECT_TRUE(options->verbose);
}

TEST(CommandLineTests, DefaultsStayWhenOptionsAreMissing)
{
    Core::Result<TestOptions> options = Parse({});
    ASSERT_TRUE(options);
    EXPECT_TRUE(options->input.empty());
    EXPECT_EQ(options->count, -1);
    EXPECT_FALSE(options->verbose);
}

TEST(CommandLineTests, MissingValueIsAnError)
{
    Core::Result<TestOptions> options = Parse({ "--count" });
    ASSERT_FALSE(options);
    EXPECT_EQ(options.GetError().GetId(), Core::EngineText::CommandLineMissingValue);
    EXPECT_NE(options.GetError().GetText().find("--count"), std::string::npos);
}

TEST(CommandLineTests, InvalidNumberIsAnError)
{
    Core::Result<TestOptions> options = Parse({ "--count", "12abc" });
    ASSERT_FALSE(options);
    EXPECT_EQ(options.GetError().GetId(), Core::EngineText::CommandLineInvalidNumber);
    EXPECT_NE(options.GetError().GetText().find("12abc"), std::string::npos);
}

TEST(CommandLineTests, UnknownFlagIsAnError)
{
    Core::Result<TestOptions> options = Parse({ "--colour", "red" });
    ASSERT_FALSE(options);
    EXPECT_EQ(options.GetError().GetId(), Core::EngineText::CommandLineUnknownOption);
    EXPECT_NE(options.GetError().GetText().find("--colour"), std::string::npos);
}

TEST(CommandLineTests, SecondPositionalArgumentIsAnError)
{
    Core::Result<TestOptions> options = Parse({ "First", "Second" });
    ASSERT_FALSE(options);
    EXPECT_EQ(options.GetError().GetId(), Core::EngineText::CommandLineExtraArgument);
    EXPECT_NE(options.GetError().GetText().find("Second"), std::string::npos);
}

TEST(CommandLineTests, UsageListsEveryOptionFromTheTable)
{
    std::string usage = Core::CommandLineUsage<TestOptions>("Tool", TestCommandLine);
    EXPECT_EQ(usage, "Usage: Tool [<path>] [--output <path>] [--count <number>] [--name <text>] [--verbose]");
}
