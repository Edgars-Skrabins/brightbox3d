#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

namespace BrightBox3D::Core
{
    enum class FileChangeType
    {
        Modified,
        Created,
        Removed,
        Renamed
    };

    struct FileChange
    {
        std::filesystem::path path;
        FileChangeType type = FileChangeType::Modified;
    };

    class FileWatcher
    {
    public:
        FileWatcher();
        ~FileWatcher();

        FileWatcher(const FileWatcher&) = delete;
        FileWatcher& operator=(const FileWatcher&) = delete;

        bool Watch(const std::filesystem::path& directory);
        void Stop();
        bool IsWatching() const;
        std::vector<FileChange> Poll();

        void Push(FileChange change);

    private:
        struct Implementation;
        std::unique_ptr<Implementation> m_implementation;
        std::mutex m_mutex;
        std::vector<FileChange> m_pending;
    };
}
