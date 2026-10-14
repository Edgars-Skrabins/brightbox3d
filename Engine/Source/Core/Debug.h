#pragma once

#include <source_location>
#include <string>

namespace BrightBox3D
{
    class Debug
    {
    public:
        static void Log(const std::string& message, std::source_location location = std::source_location::current());
        static void LogWarning(const std::string& message, std::source_location location = std::source_location::current());
        static void LogError(const std::string& message, std::source_location location = std::source_location::current());
    };
}
