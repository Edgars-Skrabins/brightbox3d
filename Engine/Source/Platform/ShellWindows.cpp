#include "Platform/Shell.h"

#include "Core/EngineText.h"
#include "Platform/ShellLaunch.h"

#include <windows.h>
#include <shellapi.h>

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

    Core::Result<void> MoveToTrash(const std::filesystem::path& path)
    {
        std::wstring from = path.wstring();
        from.push_back(L'\0');
        SHFILEOPSTRUCTW operation{};
        operation.wFunc = FO_DELETE;
        operation.pFrom = from.c_str();
        operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
        if (int result = SHFileOperationW(&operation); result != 0)
        {
            return Core::Error(Core::EngineText::TrashFailed, path.string(), std::to_string(result));
        }
        return {};
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
