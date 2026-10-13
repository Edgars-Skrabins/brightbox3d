#include "Core/CommandLine.h"

namespace BrightBox3D::Core::CommandLineText
{
    std::string UnknownOption(std::string_view option)
    {
        return "unknown option '" + std::string(option) + "'";
    }

    std::string MissingValue(std::string_view option)
    {
        return "option '" + std::string(option) + "' needs a value";
    }

    std::string InvalidNumber(std::string_view option, std::string_view value)
    {
        return "option '" + std::string(option) + "' expects a number, got '" + std::string(value) + "'";
    }

    std::string ExtraArgument(std::string_view argument)
    {
        return "unexpected argument '" + std::string(argument) + "'";
    }

    std::string Usage(std::string_view program, std::string_view options)
    {
        return "Usage: " + std::string(program) + std::string(options);
    }

    std::string_view ValueName(bool isNumber, bool isPath)
    {
        return isNumber ? "number" : (isPath ? "path" : "text");
    }
}
