#include "GameBuilder.h"

#include "Assets/AssetDatabase.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "EditorContext.h"
#include "EditorText.h"
#include "Runtime/BuildLayout.h"
#include "Runtime/GameManifest.h"

#include <utility>
#include <vector>

namespace BrightBox3D::Editor
{
    namespace
    {
        Core::Result<void, EditorError> CopyPath(const std::filesystem::path& from, const std::filesystem::path& to)
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
                return EditorError(EditorText::CopyFailed, from.generic_string(), code.message());
            }
            return {};
        }
    }

    std::filesystem::path GameBuilder::DefaultOutput(EditorContext& context)
    {
        return context.GetProjectService().GetProject().GetBuildDirectory() / Runtime::BuildLayout::PlatformFolderName();
    }

    Core::Result<void, EditorError> GameBuilder::Build(EditorContext& context, const std::filesystem::path& output)
    {
        Runtime::Project& project = context.GetProjectService().GetProject();
        if (!project.GetStartupLevel().IsValid())
        {
            return EditorError(EditorText::NoStartupLevel);
        }
        if (context.GetLevelService().IsDirty() && !context.GetLevelService().SaveLevel())
        {
            return EditorError(EditorText::SavingLevelFailed);
        }
        Core::Log::Info(Text(EditorText::BuildingGame, project.GetName(), output.generic_string()));
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        database.Refresh();
        std::vector<Core::Error> cookFailures = database.CookAll();
        if (!cookFailures.empty())
        {
            std::string cookErrors;
            for (const Core::Error& failure : cookFailures)
            {
                cookErrors += failure.GetText() + "\n";
            }
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
            return EditorError(EditorText::PlayerNotFound);
        }
        std::vector<std::pair<std::filesystem::path, std::filesystem::path>> copies = {
            { player, output / executableName },
            { engineBinaries / Runtime::BuildLayout::EngineLibraryFileName(), output / Runtime::BuildLayout::EngineLibraryFileName() },
            { database.GetCookedDirectory(), Runtime::BuildLayout::GetCookedDirectory(output) },
            { Core::FileSystem::EngineContentDirectory(), Runtime::BuildLayout::GetEngineContentDirectory(output) }
        };
        std::filesystem::path module = project.GetGameModulePath();
        if (std::filesystem::exists(module))
        {
            copies.emplace_back(module, Runtime::BuildLayout::GetGameModulePath(output, project.GetGameModuleName()));
        }
        for (const auto& [from, to] : copies)
        {
            if (Core::Result<void, EditorError> copied = CopyPath(from, to); !copied)
            {
                return copied;
            }
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
            return EditorError(EditorText::ManifestWriteFailed);
        }
        Core::Log::Info(Text(EditorText::BuildFinished, (output / executableName).generic_string()));
        return {};
    }
}
