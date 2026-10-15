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
        Error = 2,
        Count
    };

    enum class LogSource
    {
        Engine,
        Game,
        Count
    };

    enum class LogCategory
    {
        General,
        Assert,
        Shader,
        Compiler,
        Game
    };

    struct LogEntry
    {
        LogLevel level = LogLevel::Info;
        LogSource source = LogSource::Engine;
        std::string message;
        std::string timestamp;
        LogCategory category = LogCategory::General;
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
        static void Write(LogLevel level, const std::string& message, LogSource source = LogSource::Engine, LogCategory category = LogCategory::General, const std::string& file = {}, int line = 0);

        static void ForEach(const std::function<void(const LogEntry&)>& visitor);
        static void Clear();
        static uint64_t Revision();
        static int Count(LogLevel level, LogSource source);
        static int Count(LogSource source);
        static bool HasErrors(LogSource source);
        static void SetEcho(bool echo);
    };
}
