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

    std::optional<TestOptions> Parse(std::vector<std::string> arguments, std::string& error)
    {
        arguments.insert(arguments.begin(), "Tool");
        std::vector<char*> argv;
        for (std::string& argument : arguments)
        {
            argv.push_back(argument.data());
        }
        return Core::ParseCommandLine<TestOptions>(static_cast<int>(argv.size()), argv.data(), TestCommandLine, error);
    }
}

TEST(CommandLineTests, ParsesEveryValueType)
{
    std::string error;
    std::optional<TestOptions> options = Parse({ "Project", "--output", "out/shot.png", "--count", "42", "--name", "Crate", "--verbose" }, error);
    ASSERT_TRUE(options) << error;
    EXPECT_EQ(options->input, std::filesystem::path("Project"));
    EXPECT_EQ(options->output, std::filesystem::path("out/shot.png"));
    EXPECT_EQ(options->count, 42);
    EXPECT_EQ(options->name, "Crate");
    EXPECT_TRUE(options->verbose);
}

TEST(CommandLineTests, DefaultsStayWhenOptionsAreMissing)
{
    std::string error;
    std::optional<TestOptions> options = Parse({}, error);
    ASSERT_TRUE(options);
    EXPECT_TRUE(options->input.empty());
    EXPECT_EQ(options->count, -1);
    EXPECT_FALSE(options->verbose);
}

TEST(CommandLineTests, MissingValueIsAnError)
{
    std::string error;
    EXPECT_FALSE(Parse({ "--count" }, error));
    EXPECT_NE(error.find("--count"), std::string::npos);
}

TEST(CommandLineTests, InvalidNumberIsAnError)
{
    std::string error;
    EXPECT_FALSE(Parse({ "--count", "12abc" }, error));
    EXPECT_NE(error.find("12abc"), std::string::npos);
}

TEST(CommandLineTests, UnknownFlagIsAnError)
{
    std::string error;
    EXPECT_FALSE(Parse({ "--colour", "red" }, error));
    EXPECT_NE(error.find("--colour"), std::string::npos);
}

TEST(CommandLineTests, SecondPositionalArgumentIsAnError)
{
    std::string error;
    EXPECT_FALSE(Parse({ "First", "Second" }, error));
    EXPECT_NE(error.find("Second"), std::string::npos);
}

TEST(CommandLineTests, UsageListsEveryOptionFromTheTable)
{
    std::string usage = Core::CommandLineUsage<TestOptions>("Tool", TestCommandLine);
    EXPECT_EQ(usage, "Usage: Tool [<path>] [--output <path>] [--count <number>] [--name <text>] [--verbose]");
}
