#include "GameBuilder.h"

#include "Assets/AssetDatabase.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "EditorContext.h"
#include "EditorText.h"
#include "Runtime/BuildLayout.h"
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
                error = Text(EditorText::CopyFailed, from.generic_string(), code.message());
                return false;
            }
            return true;
        }
    }

    std::filesystem::path GameBuilder::DefaultOutput(EditorContext& context)
    {
        return context.GetProjectService().GetProject().GetBuildDirectory() / PlatformName();
    }

    bool GameBuilder::Build(EditorContext& context, const std::filesystem::path& output, std::string& error)
    {
        Runtime::Project& project = context.GetProjectService().GetProject();
        if (!project.GetStartupLevel().IsValid())
        {
            error = Text(EditorText::NoStartupLevel);
            return false;
        }
        if (context.GetLevelService().IsDirty() && !context.GetLevelService().SaveLevel())
        {
            error = Text(EditorText::SavingLevelFailed);
            return false;
        }
        Core::Log::Info(Text(EditorText::BuildingGame, project.GetName(), output.generic_string()));
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        database.Refresh();
        std::string cookErrors;
        if (!database.CookAll(cookErrors))
        {
            Core::Log::Warning(Text(EditorText::CookFailed, cookErrors));
        }

        std::error_code code;
        std::filesystem::remove_all(output, code);
        std::filesystem::create_directories(output, code);
        std::filesystem::path engineBinaries = Core::FileSystem::ExecutableDirectory();
        std::string executableName = Runtime::BuildLayout::PlayerExecutableFileName(project.GetGameModuleName());
        std::filesystem::path player = engineBinaries / Runtime::BuildLayout::PlayerExecutableFileName();
        if (!std::filesystem::exists(player))
        {
            error = Text(EditorText::PlayerNotFound);
            return false;
        }
        if (!CopyPath(player, output / executableName, error) ||
            !CopyPath(engineBinaries / Runtime::BuildLayout::EngineLibraryFileName(), output / Runtime::BuildLayout::EngineLibraryFileName(), error) ||
            !CopyPath(database.GetCookedDirectory(), Runtime::BuildLayout::GetCookedDirectory(output), error) ||
            !CopyPath(Core::FileSystem::EngineContentDirectory(), Runtime::BuildLayout::GetEngineContentDirectory(output), error))
        {
            return false;
        }
        std::filesystem::path module = project.GetGameModulePath();
        if (std::filesystem::exists(module) && !CopyPath(module, Runtime::BuildLayout::GetGameModulePath(output, project.GetGameModuleName()), error))
        {
            return false;
        }
        std::filesystem::permissions(output / executableName, std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec, std::filesystem::perm_options::add, code);

        Runtime::GameManifest manifest;
        manifest.AssignFromProject(project);
        if (!std::filesystem::exists(module))
        {
            manifest.gameModule.clear();
        }
        if (!manifest.Save(Runtime::BuildLayout::GetManifestPath(output)))
        {
            error = Text(EditorText::ManifestWriteFailed);
            return false;
        }
        Core::Log::Info(Text(EditorText::BuildFinished, (output / executableName).generic_string()));
        return true;
    }
}
