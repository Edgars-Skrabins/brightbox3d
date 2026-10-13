#include "Platform/Shell.h"

#include "Platform/ShellLaunch.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace BrightBox3D::Platform::Shell
{
    void OpenInCodeEditor(const std::filesystem::path& file, int line)
    {
        Launch({ "cmd", "/c", "code", "-g", file.string() + ":" + std::to_string(std::max(line, 1)) });
    }

    void RevealInFileBrowser(const std::filesystem::path& path)
    {
        Launch({ "explorer", "/select," + path.string() });
    }

    std::filesystem::path UserDirectory()
    {
        const char* home = std::getenv("HOME");
        if (!home)
        {
            home = std::getenv("USERPROFILE");
        }
        return home ? std::filesystem::path(home) : std::filesystem::path();
    }
}
