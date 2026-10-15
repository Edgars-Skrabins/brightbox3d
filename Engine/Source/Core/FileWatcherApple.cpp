#include "Core/FileWatcher.h"

#include "Core/EngineInfo.h"

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>

#include <string>

namespace BrightBox3D::Core
{
    struct FileWatcher::Implementation
    {
        FSEventStreamRef stream = nullptr;
        dispatch_queue_t queue = nullptr;
    };

    namespace
    {
        constexpr const char* FileWatcherQueueSuffix = ".FileWatcher";

        void StreamCallback(ConstFSEventStreamRef, void* context, size_t count, void* paths, const FSEventStreamEventFlags* flags, const FSEventStreamEventId*)
        {
            auto* watcher = static_cast<FileWatcher*>(context);
            auto** pathList = static_cast<char**>(paths);
            for (size_t i = 0; i < count; ++i)
            {
                FileChange change;
                change.path = pathList[i];
                FSEventStreamEventFlags flag = flags[i];
                if (flag & kFSEventStreamEventFlagItemRemoved)
                {
                    change.type = FileChangeType::Removed;
                }
                else if (flag & kFSEventStreamEventFlagItemRenamed)
                {
                    change.type = FileChangeType::Renamed;
                }
                else if (flag & kFSEventStreamEventFlagItemCreated)
                {
                    change.type = FileChangeType::Created;
                }
                else
                {
                    change.type = FileChangeType::Modified;
                }
                watcher->Push(std::move(change));
            }
        }
    }

    FileWatcher::FileWatcher() : m_implementation(std::make_unique<Implementation>())
    {
    }

    FileWatcher::~FileWatcher()
    {
        Stop();
    }

    bool FileWatcher::Watch(const std::filesystem::path& directory)
    {
        Stop();
        std::error_code error;
        std::filesystem::path absolute = std::filesystem::canonical(directory, error);
        if (error)
        {
            return false;
        }

        CFStringRef path = CFStringCreateWithCString(nullptr, absolute.c_str(), kCFStringEncodingUTF8);
        CFArrayRef paths = CFArrayCreate(nullptr, reinterpret_cast<const void**>(&path), 1, &kCFTypeArrayCallBacks);
        FSEventStreamContext context{};
        context.info = this;
        m_implementation->stream = FSEventStreamCreate(nullptr, &StreamCallback, &context, paths, kFSEventStreamEventIdSinceNow, 0.05, kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);
        CFRelease(paths);
        CFRelease(path);
        if (!m_implementation->stream)
        {
            return false;
        }

        static const std::string queueName = std::string(EngineName) + FileWatcherQueueSuffix;
        m_implementation->queue = dispatch_queue_create(queueName.c_str(), DISPATCH_QUEUE_SERIAL);
        FSEventStreamSetDispatchQueue(m_implementation->stream, m_implementation->queue);
        if (!FSEventStreamStart(m_implementation->stream))
        {
            Stop();
            return false;
        }
        return true;
    }

    void FileWatcher::Stop()
    {
        if (m_implementation->stream)
        {
            FSEventStreamStop(m_implementation->stream);
            FSEventStreamInvalidate(m_implementation->stream);
            FSEventStreamRelease(m_implementation->stream);
            m_implementation->stream = nullptr;
        }
        if (m_implementation->queue)
        {
            dispatch_sync_f(m_implementation->queue, nullptr, [](void*) {});
            dispatch_release(m_implementation->queue);
            m_implementation->queue = nullptr;
        }
    }

    bool FileWatcher::IsWatching() const
    {
        return m_implementation->stream != nullptr;
    }

    std::vector<FileChange> FileWatcher::Poll()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<FileChange> changes;
        changes.swap(m_pending);
        return changes;
    }

    void FileWatcher::Push(FileChange change)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending.push_back(std::move(change));
    }
}
