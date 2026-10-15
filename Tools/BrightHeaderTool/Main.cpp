#include "Generator.h"
#include "Parser.h"
#include "Tokenizer.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    bool ReadFile(const std::filesystem::path& path, std::string& text)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return false;
        }
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        text = buffer.str();
        return true;
    }

    bool WriteIfChanged(const std::filesystem::path& path, const std::string& text)
    {
        std::string existing;
        if (ReadFile(path, existing) && existing == text)
        {
            std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now());
            return true;
        }
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return false;
        }
        stream << text;
        return static_cast<bool>(stream);
    }

    std::string MakeIdentifier(const std::string& text)
    {
        std::string result;
        for (char c : text)
        {
            result += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
        }
        return result;
    }

    constexpr const char* HeaderFlag = "--header";
    constexpr const char* OutputFlag = "--output";
    constexpr const char* IncludeFlag = "--include";

    void PrintUsage()
    {
        std::fprintf(stderr, "Usage: BrightHeaderTool %s <file.h> %s <include path> %s <file.gen.cpp>\n", HeaderFlag, IncludeFlag, OutputFlag);
    }
}

int main(int argc, char** argv)
{
    std::filesystem::path header;
    std::filesystem::path output;
    std::string include;

    for (int i = 1; i < argc; ++i)
    {
        std::string argument = argv[i];
        if (argument == HeaderFlag && i + 1 < argc)
        {
            header = argv[++i];
        }
        else if (argument == OutputFlag && i + 1 < argc)
        {
            output = argv[++i];
        }
        else if (argument == IncludeFlag && i + 1 < argc)
        {
            include = argv[++i];
        }
        else
        {
            PrintUsage();
            return 2;
        }
    }

    if (header.empty() || output.empty() || include.empty())
    {
        PrintUsage();
        return 2;
    }

    std::string source;
    if (!ReadFile(header, source))
    {
        std::fprintf(stderr, "%s: error: cannot read header\n", header.string().c_str());
        return 1;
    }

    auto tokens = BrightBox3D::HeaderTool::Tokenize(source);
    auto parsed = BrightBox3D::HeaderTool::Parse(tokens);
    auto generated = BrightBox3D::HeaderTool::Generate(parsed, include, MakeIdentifier(include));

    std::vector<BrightBox3D::HeaderTool::Diagnostic> errors = parsed.errors;
    errors.insert(errors.end(), generated.errors.begin(), generated.errors.end());
    if (!errors.empty())
    {
        for (const auto& error : errors)
        {
            std::fprintf(stderr, "%s:%d: error: %s\n", header.string().c_str(), error.line, error.message.c_str());
        }
        return 1;
    }

    if (!WriteIfChanged(output, generated.code))
    {
        std::fprintf(stderr, "%s: error: cannot write output\n", output.string().c_str());
        return 1;
    }
    return 0;
}
