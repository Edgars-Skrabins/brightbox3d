#include "Core/Debug.h"

#include "Core/Log.h"

namespace BrightBox3D
{
    namespace
    {
        constexpr const char* GameCategory = "Game";

        void WriteGame(Core::LogLevel level, const std::string& message, const std::source_location& location)
        {
            Core::Log::Write(level, message, Core::LogSource::Game, GameCategory, location.file_name(), static_cast<int>(location.line()));
        }
    }

    void Debug::Log(const std::string& message, std::source_location location)
    {
        WriteGame(Core::LogLevel::Info, message, location);
    }

    void Debug::LogWarning(const std::string& message, std::source_location location)
    {
        WriteGame(Core::LogLevel::Warning, message, location);
    }

    void Debug::LogError(const std::string& message, std::source_location location)
    {
        WriteGame(Core::LogLevel::Error, message, location);
    }
}
