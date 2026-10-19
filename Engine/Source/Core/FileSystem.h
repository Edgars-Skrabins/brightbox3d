#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace BrightBox3D::Core
{
    class FileSystem
    {
    public:
        static constexpr const char* TempExtension = ".tmp";

        static std::optional<std::string> ReadText(const std::filesystem::path& path);
        static std::optional<std::vector<uint8_t>> ReadBinary(const std::filesystem::path& path);
        static bool WriteText(const std::filesystem::path& path, const std::string& text);
        static bool WriteBinary(const std::filesystem::path& path, const void* data, size_t size);
        static bool EnsureDirectory(const std::filesystem::path& path);
        static std::filesystem::path ExecutableDirectory();
        static std::filesystem::path EngineDirectory();
        static std::filesystem::path EngineContentDirectory();
        static std::optional<std::filesystem::path> EngineSourceDirectory();
        static std::optional<std::filesystem::path> FindContentDirectory(const std::filesystem::path& installedFolder, const std::filesystem::path& sourceTreeFolder);
        static std::string ToGenericString(const std::filesystem::path& path);
        static int64_t LastWriteTime(const std::filesystem::path& path);
    };
}
