#pragma once

#include "Core/FileWatcher.h"

#include <filesystem>

namespace BrightBox3D::Editor
{
    class HotReload;
    class ProjectService;

    class FileChangeRouter
    {
    public:
        void Watch(const std::filesystem::path& root);
        void Stop();
        void Process(const ProjectService& projects, HotReload& hotReload);

    private:
        Core::FileWatcher m_watcher;
    };
}
