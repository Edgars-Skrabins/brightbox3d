#include "Core/CommandLine.h"

#include "Core/EngineText.h"

namespace BrightBox3D::Core::CommandLineText
{
    std::string Usage(std::string_view program, std::string_view options)
    {
        return Text(EngineText::CommandLineUsage, program, options);
    }

    std::string_view ValueName(bool isNumber, bool isPath)
    {
        return isNumber ? Text(EngineText::CommandLineNumberValue) : (isPath ? Text(EngineText::CommandLinePathValue) : Text(EngineText::CommandLineTextValue));
    }
}
