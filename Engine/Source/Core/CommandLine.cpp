#include "Core/CommandLine.h"

#include "Core/EngineText.h"

namespace BrightBox3D::Core::CommandLineText
{
    std::string UnknownOption(std::string_view option)
    {
        return Text(EngineText::CommandLineUnknownOption, option);
    }

    std::string MissingValue(std::string_view option)
    {
        return Text(EngineText::CommandLineMissingValue, option);
    }

    std::string InvalidNumber(std::string_view option, std::string_view value)
    {
        return Text(EngineText::CommandLineInvalidNumber, option, value);
    }

    std::string ExtraArgument(std::string_view argument)
    {
        return Text(EngineText::CommandLineExtraArgument, argument);
    }

    std::string Usage(std::string_view program, std::string_view options)
    {
        return Text(EngineText::CommandLineUsage, program, options);
    }

    std::string_view ValueName(bool isNumber, bool isPath)
    {
        return isNumber ? Text(EngineText::CommandLineNumberValue) : (isPath ? Text(EngineText::CommandLinePathValue) : Text(EngineText::CommandLineTextValue));
    }
}
