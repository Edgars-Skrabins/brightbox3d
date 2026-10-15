#pragma once

#include "Core/EngineInfo.h"

#include <filesystem>
#include <string>

namespace BrightBox3D::Runtime::BuildLayout
{
    inline constexpr const char* PlayerExecutableName = "BrightPlayer";
    inline constexpr const char* EngineLibraryName = Core::EngineModuleName;

    const char* PlatformFolderName();
    std::string PlayerExecutableFileName(const std::string& baseName = PlayerExecutableName);
    std::string EngineLibraryFileName();
    std::filesystem::path GetCookedDirectory(const std::filesystem::path& buildRoot);
    std::filesystem::path GetEngineContentDirectory(const std::filesystem::path& buildRoot);
    std::filesystem::path GetGameModulePath(const std::filesystem::path& buildRoot, const std::string& gameModule);
    std::filesystem::path GetManifestPath(const std::filesystem::path& buildRoot);
}
