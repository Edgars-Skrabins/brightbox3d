#include "Core/FileSystem.h"
#include "Core/Process.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace BrightBox3D;

namespace
{
    std::filesystem::path SnippetPath(const std::string& name)
    {
        return std::filesystem::path(BRIGHT_COMPILE_ERROR_SNIPPETS) / (name + ".snippet");
    }

    Core::ProcessResult CompileSnippet(const std::string& name)
    {
        std::vector<std::string> arguments = { BRIGHT_CXX_COMPILER };
#if defined(_MSC_VER)
        arguments.insert(arguments.end(), { "/nologo", "/std:c++20", "/Zs", "/TP", "/I" BRIGHT_ENGINE_SOURCE_DIR, "/I" BRIGHT_ENGINE_GENERATED_DIR });
#else
        arguments.insert(arguments.end(), { "-std=c++20", "-fsyntax-only", "-x", "c++", "-Werror=reorder-init-list", "-I" BRIGHT_ENGINE_SOURCE_DIR, "-I" BRIGHT_ENGINE_GENERATED_DIR });
        if (std::string(BRIGHT_OSX_SYSROOT).size() > 0)
        {
            arguments.insert(arguments.end(), { "-isysroot", BRIGHT_OSX_SYSROOT });
        }
#endif
        arguments.push_back(SnippetPath(name).string());
        return Core::Process::Run(arguments);
    }

    int LineOf(const std::string& name, const std::string& text)
    {
        std::string source = Core::FileSystem::ReadText(SnippetPath(name)).value_or("");
        size_t position = source.find(text);
        return position == std::string::npos ? -1 : 1 + static_cast<int>(std::count(source.begin(), source.begin() + static_cast<std::ptrdiff_t>(position), '\n'));
    }

    void ExpectErrorAtMacro(const std::string& name)
    {
        Core::ProcessResult result = CompileSnippet(name);
        ASSERT_TRUE(result.started);
        EXPECT_NE(result.exitCode, 0) << name << " compiled but should not";
        std::string line = std::to_string(LineOf(name, "BrightProperty("));
#if defined(_MSC_VER)
        std::string location = name + ".snippet(" + line + ")";
#else
        std::string location = name + ".snippet:" + line;
#endif
        EXPECT_NE(result.output.find(location), std::string::npos) << result.output;
    }
}

TEST(CompileErrorTests, ValidMetadataCompiles)
{
    Core::ProcessResult result = CompileSnippet("Valid");
    ASSERT_TRUE(result.started);
    EXPECT_EQ(result.exitCode, 0) << result.output;
}

TEST(CompileErrorTests, UnknownFieldIsAnError)
{
    ExpectErrorAtMacro("UnknownField");
}

TEST(CompileErrorTests, MisspelledEnumValueIsAnError)
{
    ExpectErrorAtMacro("MisspelledEnumValue");
}

TEST(CompileErrorTests, OutOfOrderFieldsAreAnError)
{
    ExpectErrorAtMacro("OutOfOrderFields");
}

TEST(CompileErrorTests, StringCategoryIsAnError)
{
    ExpectErrorAtMacro("StringCategory");
}

TEST(CompileErrorTests, HeaderToolRejectsOldStyleSpecifiers)
{
    std::filesystem::path output = std::filesystem::temp_directory_path() / "BrightTestsCompileErrors" / "OldStyleSpecifier.gen.cpp";
    std::filesystem::create_directories(output.parent_path());
    Core::ProcessResult result = Core::Process::Run({ BRIGHT_HEADER_TOOL, "--header", SnippetPath("OldStyleSpecifier").string(), "--include", "OldStyleSpecifier.h", "--output", output.string() });
    ASSERT_TRUE(result.started);
    EXPECT_NE(result.exitCode, 0);
    std::string location = "OldStyleSpecifier.snippet:" + std::to_string(LineOf("OldStyleSpecifier", "BrightProperty(")) + ": error: Old-style BrightProperty argument 'EditAnywhere'";
    EXPECT_NE(result.output.find(location), std::string::npos) << result.output;
}
