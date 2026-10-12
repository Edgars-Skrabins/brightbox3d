#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace BrightBox3D::Core
{
    enum class LogLevel : int
    {
        Info = 0,
        Warning = 1,
        Error = 2
    };

    struct LogEntry
    {
        LogLevel level = LogLevel::Info;
        std::string message;
        std::string timestamp;
        std::string category;
        std::string file;
        int line = 0;
        uint64_t serial = 0;
    };

    class Log
    {
    public:
        static void Info(const std::string& message);
        static void Warning(const std::string& message);
        static void Error(const std::string& message);
        static void Write(LogLevel level, const std::string& message, const std::string& category = {}, const std::string& file = {}, int line = 0);

        static void ForEach(const std::function<void(const LogEntry&)>& visitor);
        static void Clear();
        static uint64_t Revision();
        static int Count(LogLevel level);
        static void SetEcho(bool echo);
    };
}
