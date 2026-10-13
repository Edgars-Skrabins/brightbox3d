#pragma once

#include <charconv>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace BrightBox3D::Core
{
    template <typename Options>
    struct CommandLineOption
    {
        using Member = std::variant<bool Options::*, int Options::*, std::string Options::*, std::filesystem::path Options::*>;

        const char* flag = nullptr;
        Member member;
    };

    namespace CommandLineText
    {
        std::string UnknownOption(std::string_view option);
        std::string MissingValue(std::string_view option);
        std::string InvalidNumber(std::string_view option, std::string_view value);
        std::string ExtraArgument(std::string_view argument);
        std::string Usage(std::string_view program, std::string_view options);
        std::string_view ValueName(bool isNumber, bool isPath);
    }

    inline void AssignText(bool&, std::string_view)
    {
    }

    inline void AssignText(int&, std::string_view)
    {
    }

    inline void AssignText(std::string& target, std::string_view value)
    {
        target = value;
    }

    inline void AssignText(std::filesystem::path& target, std::string_view value)
    {
        target = std::filesystem::path(value);
    }

    template <typename Options>
    bool IsPositional(const CommandLineOption<Options>& option)
    {
        return option.flag == nullptr;
    }

    template <typename Options>
    std::optional<Options> ParseCommandLine(int argc, char** argv, std::span<const CommandLineOption<Options>> table, std::string& error)
    {
        Options options;
        bool positionalUsed = false;
        for (int index = 1; index < argc; ++index)
        {
            std::string_view argument = argv[index];
            const CommandLineOption<Options>* match = nullptr;
            for (const CommandLineOption<Options>& option : table)
            {
                bool positionalMatch = IsPositional(option) && !argument.starts_with("--") && !positionalUsed;
                if (positionalMatch || (!IsPositional(option) && argument == option.flag))
                {
                    match = &option;
                    break;
                }
            }
            if (!match)
            {
                error = argument.starts_with("--") ? CommandLineText::UnknownOption(argument) : CommandLineText::ExtraArgument(argument);
                return std::nullopt;
            }
            if (IsPositional(*match))
            {
                positionalUsed = true;
                std::visit([&](auto member) { AssignText(options.*member, argument); }, match->member);
                continue;
            }
            if (std::holds_alternative<bool Options::*>(match->member))
            {
                options.*std::get<bool Options::*>(match->member) = true;
                continue;
            }
            if (index + 1 >= argc)
            {
                error = CommandLineText::MissingValue(argument);
                return std::nullopt;
            }
            std::string_view value = argv[++index];
            if (int Options::* const* number = std::get_if<int Options::*>(&match->member))
            {
                int parsed = 0;
                auto [end, code] = std::from_chars(value.data(), value.data() + value.size(), parsed);
                if (code != std::errc() || end != value.data() + value.size())
                {
                    error = CommandLineText::InvalidNumber(argument, value);
                    return std::nullopt;
                }
                options.**number = parsed;
                continue;
            }
            std::visit([&](auto member) { AssignText(options.*member, value); }, match->member);
        }
        return options;
    }

    template <typename Options>
    std::string CommandLineUsage(std::string_view program, std::span<const CommandLineOption<Options>> table)
    {
        std::string options;
        for (const CommandLineOption<Options>& option : table)
        {
            bool isFlag = std::holds_alternative<bool Options::*>(option.member);
            bool isNumber = std::holds_alternative<int Options::*>(option.member);
            bool isPath = std::holds_alternative<std::filesystem::path Options::*>(option.member);
            std::string value = isFlag ? std::string() : "<" + std::string(CommandLineText::ValueName(isNumber, isPath)) + ">";
            options += " [" + (IsPositional(option) ? value : std::string(option.flag) + (value.empty() ? "" : " " + value)) + "]";
        }
        return CommandLineText::Usage(program, options);
    }
}
