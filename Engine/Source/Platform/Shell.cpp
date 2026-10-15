#include "Platform/ShellLaunch.h"

#include "Core/Process.h"
#include "Platform/Shell.h"

#include <algorithm>
#include <memory>
#include <mutex>

namespace BrightBox3D::Platform::Shell
{
    namespace
    {
        struct Launches
        {
            std::mutex mutex;
            std::vector<std::unique_ptr<Core::AsyncProcess>> processes;
        };

        Launches& GetLaunches()
        {
            static Launches* launches = new Launches();
            return *launches;
        }
    }

    void Launch(std::vector<std::string> command)
    {
        Launches& launches = GetLaunches();
        std::lock_guard<std::mutex> lock(launches.mutex);
        std::erase_if(launches.processes, [](const std::unique_ptr<Core::AsyncProcess>& process) { return process->HasResult(); });
        launches.processes.push_back(std::make_unique<Core::AsyncProcess>());
        launches.processes.back()->Start({ std::move(command) });
    }

    void Shutdown()
    {
        Launches& launches = GetLaunches();
        std::lock_guard<std::mutex> lock(launches.mutex);
        launches.processes.clear();
    }
}
