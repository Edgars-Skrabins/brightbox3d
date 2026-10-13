#include "Platform/Shell.h"

#include "Platform/ShellLaunch.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace BrightBox3D::Platform::Shell
{
    namespace
    {
        const char* CodeEditorCandidates[] = { "/usr/local/bin/code", "/opt/homebrew/bin/code" };
    }

    void OpenInCodeEditor(const std::filesystem::path& file, int line)
    {
        for (const char* candidate : CodeEditorCandidates)
        {
            if (std::filesystem::exists(candidate))
            {
                Launch({ candidate, "-g", file.string() + ":" + std::to_string(std::max(line, 1)) });
                return;
            }
        }
        Launch({ "open", "-t", file.string() });
    }

    void RevealInFileBrowser(const std::filesystem::path& path)
    {
        Launch({ "open", "-R", path.string() });
    }

    std::filesystem::path UserDirectory()
    {
        const char* home = std::getenv("HOME");
        return home ? std::filesystem::path(home) : std::filesystem::path();
    }
}
