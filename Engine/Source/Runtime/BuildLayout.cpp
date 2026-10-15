#include "Runtime/BuildLayout.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineLayout.h"
#include "Core/FileSystem.h"
#include "Core/SharedLibrary.h"
#include "Runtime/GameManifest.h"

namespace BrightBox3D::Runtime::BuildLayout
{
    namespace
    {
#if defined(_WIN32)
        constexpr const char* ExecutableExtension = ".exe";
        constexpr const char* PlatformFolder = "Windows";
#elif defined(__APPLE__)
        constexpr const char* ExecutableExtension = "";
        constexpr const char* PlatformFolder = "macOS";
#else
        constexpr const char* ExecutableExtension = "";
        constexpr const char* PlatformFolder = "Linux";
#endif
    }

    const char* PlatformFolderName()
    {
        return PlatformFolder;
    }

    std::string PlayerExecutableFileName(const std::string& baseName)
    {
        return baseName + ExecutableExtension;
    }

    std::string EngineLibraryFileName()
    {
        return Core::SharedLibrary::PlatformFileName(EngineLibraryName);
    }

    std::filesystem::path GetCookedDirectory(const std::filesystem::path& buildRoot)
    {
        return buildRoot / Assets::AssetDatabase::CookedFolder;
    }

    std::filesystem::path GetEngineContentDirectory(const std::filesystem::path& buildRoot)
    {
        return buildRoot / Core::EngineLayout::ContentFolder;
    }

    std::filesystem::path GetGameModulePath(const std::filesystem::path& buildRoot, const std::string& gameModule)
    {
        return buildRoot / Core::SharedLibrary::PlatformFileName(gameModule);
    }

    std::filesystem::path GetManifestPath(const std::filesystem::path& buildRoot)
    {
        return buildRoot / GameManifest::FileName;
    }
}
