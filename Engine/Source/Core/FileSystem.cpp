#include "Core/FileSystem.h"

#include <fstream>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace BrightBox3D::Core
{
    std::optional<std::string> FileSystem::ReadText(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return std::nullopt;
        }
        std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        return text;
    }

    std::optional<std::vector<uint8_t>> FileSystem::ReadBinary(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            return std::nullopt;
        }
        std::streamsize size = stream.tellg();
        stream.seekg(0, std::ios::beg);
        std::vector<uint8_t> data(static_cast<size_t>(size));
        if (size > 0 && !stream.read(reinterpret_cast<char*>(data.data()), size))
        {
            return std::nullopt;
        }
        return data;
    }

    bool FileSystem::WriteText(const std::filesystem::path& path, const std::string& text)
    {
        return WriteBinary(path, text.data(), text.size());
    }

    bool FileSystem::WriteBinary(const std::filesystem::path& path, const void* data, size_t size)
    {
        if (path.has_parent_path() && !EnsureDirectory(path.parent_path()))
        {
            return false;
        }
        std::filesystem::path temporary = path;
        temporary += ".tmp";
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                return false;
            }
            stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
            if (!stream)
            {
                return false;
            }
        }
        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            std::filesystem::remove(path, error);
            std::filesystem::rename(temporary, path, error);
        }
        return !error;
    }

    bool FileSystem::EnsureDirectory(const std::filesystem::path& path)
    {
        std::error_code error;
        if (std::filesystem::is_directory(path, error))
        {
            return true;
        }
        std::filesystem::create_directories(path, error);
        return !error;
    }

    std::filesystem::path FileSystem::ExecutableDirectory()
    {
#if defined(_WIN32)
        wchar_t buffer[MAX_PATH];
        DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return std::filesystem::path(std::wstring(buffer, length)).parent_path();
#elif defined(__APPLE__)
        char buffer[4096];
        uint32_t size = sizeof(buffer);
        if (_NSGetExecutablePath(buffer, &size) != 0)
        {
            return std::filesystem::current_path();
        }
        std::error_code error;
        std::filesystem::path path = std::filesystem::canonical(buffer, error);
        return error ? std::filesystem::path(buffer).parent_path() : path.parent_path();
#else
        char buffer[4096];
        ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (length <= 0)
        {
            return std::filesystem::current_path();
        }
        return std::filesystem::path(std::string(buffer, static_cast<size_t>(length))).parent_path();
#endif
    }

    std::filesystem::path FileSystem::EngineDirectory()
    {
        std::filesystem::path directory = ExecutableDirectory();
        for (int i = 0; i < 6 && !directory.empty(); ++i)
        {
            if (std::filesystem::exists(directory / EngineContentFolder))
            {
                return directory;
            }
            if (std::filesystem::exists(directory / SourceTreeEngineFolder / SourceTreeContentFolder))
            {
                return directory / SourceTreeEngineFolder;
            }
            if (!directory.has_parent_path() || directory.parent_path() == directory)
            {
                break;
            }
            directory = directory.parent_path();
        }
        return ExecutableDirectory();
    }

    std::filesystem::path FileSystem::EngineContentDirectory()
    {
        std::filesystem::path directory = ExecutableDirectory();
        for (int i = 0; i < 6 && !directory.empty(); ++i)
        {
            if (std::filesystem::exists(directory / EngineContentFolder))
            {
                return directory / EngineContentFolder;
            }
            if (std::filesystem::exists(directory / SourceTreeEngineFolder / SourceTreeContentFolder))
            {
                return directory / SourceTreeEngineFolder / SourceTreeContentFolder;
            }
            if (!directory.has_parent_path() || directory.parent_path() == directory)
            {
                break;
            }
            directory = directory.parent_path();
        }
        return ExecutableDirectory() / EngineContentFolder;
    }

    std::string FileSystem::ToGenericString(const std::filesystem::path& path)
    {
        return path.generic_string();
    }

    int64_t FileSystem::LastWriteTime(const std::filesystem::path& path)
    {
        std::error_code error;
        auto time = std::filesystem::last_write_time(path, error);
        if (error)
        {
            return 0;
        }
        return static_cast<int64_t>(time.time_since_epoch().count());
    }
}
