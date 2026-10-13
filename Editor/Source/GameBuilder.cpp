#include "GameBuilder.h"

#include "Assets/AssetDatabase.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/SharedLibrary.h"
#include "EditorContext.h"
#include "Runtime/GameManifest.h"

namespace BrightBox3D::Editor
{
    namespace
    {
        const char* PlatformName()
        {
#if defined(_WIN32)
            return "Windows";
#elif defined(__APPLE__)
            return "macOS";
#else
            return "Linux";
#endif
        }

        bool CopyPath(const std::filesystem::path& from, const std::filesystem::path& to, std::string& error)
        {
            std::error_code code;
            if (std::filesystem::is_directory(from))
            {
                std::filesystem::create_directories(to, code);
                std::filesystem::copy(from, to, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, code);
            }
            else
            {
                std::filesystem::create_directories(to.parent_path(), code);
                std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, code);
            }
            if (code)
            {
                error = "copy '" + from.generic_string() + "' failed: " + code.message();
                return false;
            }
            return true;
        }
    }

    std::filesystem::path GameBuilder::DefaultOutput(EditorContext& context)
    {
        return context.GetProject().GetRoot() / "Build" / PlatformName();
    }

    bool GameBuilder::Build(EditorContext& context, const std::filesystem::path& output, std::string& error)
    {
        Runtime::Project& project = context.GetProject();
        if (!project.GetStartupLevel().IsValid())
        {
            error = "the project has no startup level";
            return false;
        }
        if (context.IsDirty() && !context.SaveLevel())
        {
            error = "saving the level failed";
            return false;
        }
        Core::Log::Info("Building '" + project.GetName() + "' into " + output.generic_string());
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        database.Refresh();
        std::string cookErrors;
        if (!database.CookAll(cookErrors))
        {
            Core::Log::Warning("Some assets failed to cook:\n" + cookErrors);
        }

        std::error_code code;
        std::filesystem::remove_all(output, code);
        std::filesystem::create_directories(output, code);
        std::filesystem::path engineBinaries = Core::FileSystem::ExecutableDirectory();
        std::string executableName = project.GetGameModuleName();
#if defined(_WIN32)
        std::filesystem::path player = engineBinaries / "BrightPlayer.exe";
        executableName += ".exe";
#else
        std::filesystem::path player = engineBinaries / "BrightPlayer";
#endif
        if (!std::filesystem::exists(player))
        {
            error = "BrightPlayer was not found next to the editor";
            return false;
        }
        if (!CopyPath(player, output / executableName, error) ||
            !CopyPath(engineBinaries / Core::SharedLibrary::PlatformFileName("BrightBox3D"), output / Core::SharedLibrary::PlatformFileName("BrightBox3D"), error) ||
            !CopyPath(database.GetCookedDirectory(), output / "Cooked", error) ||
            !CopyPath(Core::FileSystem::EngineContentDirectory(), output / "EngineContent", error))
        {
            return false;
        }
        std::filesystem::path module = project.GetRoot() / "Binaries" / Core::SharedLibrary::PlatformFileName(project.GetGameModuleName());
        if (std::filesystem::exists(module) && !CopyPath(module, output / module.filename(), error))
        {
            return false;
        }
        std::filesystem::permissions(output / executableName, std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec, std::filesystem::perm_options::add, code);

        Runtime::GameManifest manifest;
        manifest.name = project.GetName();
        manifest.gameModule = std::filesystem::exists(module) ? project.GetGameModuleName() : std::string();
        manifest.startupLevel = project.GetStartupLevel();
        manifest.fixedTimeStep = project.GetFixedTimeStep();
        manifest.gameMode.CopyFrom(project.GetGameMode(), manifest.gameMode.GetAllocator());
        if (!manifest.Save(output / Runtime::GameManifest::FileName))
        {
            error = "writing Game.json failed";
            return false;
        }
        Core::Log::Info("Build finished: " + (output / executableName).generic_string());
        return true;
    }
}
