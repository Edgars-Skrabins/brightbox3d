#include "FileChangeRouter.h"

#include "Assets/AssetDatabase.h"
#include "Core/StringUtils.h"
#include "HotReload.h"
#include "Services/ScriptLibrary.h"
#include "Services/ProjectService.h"

#include <string>
#include <vector>

namespace BrightBox3D::Editor
{
    namespace
    {
        struct WatchedRoot
        {
            std::string path;
            std::string canonical;

            explicit WatchedRoot(const std::filesystem::path& directory)
            {
                std::error_code error;
                path = directory.generic_string();
                canonical = std::filesystem::weakly_canonical(directory, error).generic_string();
            }

            bool Contains(const std::string& file) const
            {
                return Core::StartsWith(file, path) || Core::StartsWith(file, canonical);
            }
        };
    }

    void FileChangeRouter::Watch(const std::filesystem::path& root)
    {
        m_watcher.Watch(root);
    }

    void FileChangeRouter::Stop()
    {
        m_watcher.Stop();
    }

    void FileChangeRouter::Process(const ProjectService& projects, HotReload& hotReload, ScriptLibrary& scripts)
    {
        std::vector<Core::FileChange> changes = m_watcher.Poll();
        if (changes.empty() || !projects.HasProject())
        {
            return;
        }
        WatchedRoot scriptsRoot(projects.GetProject().GetScriptsDirectory());
        WatchedRoot assets(projects.GetProject().GetAssetsDirectory());
        bool assetsChanged = false;
        for (const Core::FileChange& change : changes)
        {
            std::string path = change.path.generic_string();
            if (scriptsRoot.Contains(path))
            {
                hotReload.NotifySourceChanged(change.path);
                scripts.MarkDirty();
            }
            else if (assets.Contains(path) && !Assets::AssetDatabase::IsIgnoredPath(change.path))
            {
                assetsChanged = true;
            }
        }
        if (assetsChanged)
        {
            Assets::AssetDatabase::Get().Refresh();
        }
    }
}
