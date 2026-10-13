#include "Platform/ShellLaunch.h"

#include "Core/Process.h"

#include <thread>

namespace BrightBox3D::Platform::Shell
{
    void Launch(std::vector<std::string> command)
    {
        std::thread([command = std::move(command)] { Core::Process::Run(command); }).detach();
    }
}
