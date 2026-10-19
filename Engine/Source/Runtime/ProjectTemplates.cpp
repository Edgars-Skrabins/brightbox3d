#include "Runtime/ProjectTemplates.h"

#include "Core/FileSystem.h"
#include "Core/TemplateFile.h"
#include "Runtime/ProjectLayout.h"

namespace BrightBox3D::Runtime::ProjectTemplates
{
    namespace
    {
        constexpr const char* TemplatesFolder = "Templates";
        constexpr const char* GameCMakeListsTemplate = "GameCMakeLists.txt.in";
        constexpr const char* GitIgnoreTemplate = "ProjectGitignore.in";

        std::filesystem::path TemplatePath(const char* name)
        {
            return Core::FileSystem::EngineContentDirectory() / TemplatesFolder / name;
        }
    }

    std::optional<std::string> GameCMakeLists(const std::string& moduleName, const std::filesystem::path& sdk)
    {
        const Core::TemplateValue values[] = {
            { "NAME", moduleName },
            { "SDK", Core::FileSystem::ToGenericString(sdk) },
            { "SCRIPTS", ProjectLayout::ScriptsFolder },
            { "STAGING", (std::filesystem::path(ProjectLayout::BinariesFolder) / ProjectLayout::StagingFolder).generic_string() },
        };
        return Core::RenderTemplate(TemplatePath(GameCMakeListsTemplate), values);
    }

    std::optional<std::string> GitIgnore()
    {
        const Core::TemplateValue values[] = {
            { "CACHE", ProjectLayout::CacheFolder },
            { "BUILD", ProjectLayout::BuildFolder },
            { "BINARIES", ProjectLayout::BinariesFolder },
            { "CMAKE_LISTS", ProjectLayout::CMakeListsFileName },
        };
        return Core::RenderTemplate(TemplatePath(GitIgnoreTemplate), values);
    }
}
