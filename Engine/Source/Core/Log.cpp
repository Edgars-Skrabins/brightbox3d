#include "Core/Log.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <deque>

namespace BrightBox3D::Core
{
    namespace
    {
        constexpr size_t MaxEntries = 10000;

        std::mutex& Mutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        struct LogState
        {
            std::deque<LogEntry> entries;
            uint64_t serial = 0;
            uint64_t revision = 0;
            int counts[3] = { 0, 0, 0 };
            bool echo = true;
        };

        LogState& State()
        {
            static LogState state;
            return state;
        }

        std::string CurrentTimestamp()
        {
            auto now = std::chrono::system_clock::now();
            std::time_t time = std::chrono::system_clock::to_time_t(now);
            std::tm local{};
#if defined(_WIN32)
            localtime_s(&local, &time);
#else
            localtime_r(&time, &local);
#endif
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
            return buffer;
        }
    }

    void Log::Info(const std::string& message)
    {
        Write(LogLevel::Info, message);
    }

    void Log::Warning(const std::string& message)
    {
        Write(LogLevel::Warning, message);
    }

    void Log::Error(const std::string& message)
    {
        Write(LogLevel::Error, message);
    }

    void Log::Write(LogLevel level, const std::string& message, const std::string& category, const std::string& file, int line)
    {
        std::lock_guard<std::mutex> lock(Mutex());
        LogState& state = State();

        LogEntry entry;
        entry.level = level;
        entry.message = message;
        entry.timestamp = CurrentTimestamp();
        entry.category = category;
        entry.file = file;
        entry.line = line;
        entry.serial = ++state.serial;

        if (state.echo)
        {
            static const char* labels[] = { "Info", "Warning", "Error" };
            std::FILE* stream = level == LogLevel::Error ? stderr : stdout;
            if (file.empty())
            {
                std::fprintf(stream, "[%s] [%s] %s\n", entry.timestamp.c_str(), labels[static_cast<int>(level)], message.c_str());
            }
            else
            {
                std::fprintf(stream, "[%s] [%s] %s:%d: %s\n", entry.timestamp.c_str(), labels[static_cast<int>(level)], file.c_str(), line, message.c_str());
            }
            std::fflush(stream);
        }

        state.counts[static_cast<int>(level)]++;
        state.entries.push_back(std::move(entry));
        if (state.entries.size() > MaxEntries)
        {
            state.counts[static_cast<int>(state.entries.front().level)]--;
            state.entries.pop_front();
        }
        state.revision++;
    }

    void Log::ForEach(const std::function<void(const LogEntry&)>& visitor)
    {
        std::lock_guard<std::mutex> lock(Mutex());
        for (const LogEntry& entry : State().entries)
        {
            visitor(entry);
        }
    }

    void Log::Clear()
    {
        std::lock_guard<std::mutex> lock(Mutex());
        LogState& state = State();
        state.entries.clear();
        state.counts[0] = state.counts[1] = state.counts[2] = 0;
        state.revision++;
    }

    uint64_t Log::Revision()
    {
        std::lock_guard<std::mutex> lock(Mutex());
        return State().revision;
    }

    int Log::Count(LogLevel level)
    {
        std::lock_guard<std::mutex> lock(Mutex());
        return State().counts[static_cast<int>(level)];
    }

    void Log::SetEcho(bool echo)
    {
        std::lock_guard<std::mutex> lock(Mutex());
        State().echo = echo;
    }
}
