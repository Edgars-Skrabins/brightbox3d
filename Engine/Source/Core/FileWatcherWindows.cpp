#include "Core/FileWatcher.h"

#include <atomic>
#include <thread>
#include <windows.h>

namespace BrightBox3D::Core
{
    struct FileWatcher::Implementation
    {
        HANDLE directory = INVALID_HANDLE_VALUE;
        HANDLE stopEvent = nullptr;
        std::thread thread;
        std::filesystem::path root;
    };

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
        m_implementation->root = std::filesystem::canonical(directory, error);
        if (error)
        {
            return false;
        }
        m_implementation->directory = CreateFileW(m_implementation->root.wstring().c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        if (m_implementation->directory == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        m_implementation->stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        m_implementation->thread = std::thread([this]
        {
            alignas(DWORD) char buffer[64 * 1024];
            OVERLAPPED overlapped{};
            overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            while (true)
            {
                ResetEvent(overlapped.hEvent);
                DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;
                if (!ReadDirectoryChangesW(m_implementation->directory, buffer, sizeof(buffer), TRUE, filter, nullptr, &overlapped, nullptr))
                {
                    break;
                }
                HANDLE handles[2] = { overlapped.hEvent, m_implementation->stopEvent };
                DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                if (wait != WAIT_OBJECT_0)
                {
                    CancelIo(m_implementation->directory);
                    break;
                }
                DWORD bytes = 0;
                if (!GetOverlappedResult(m_implementation->directory, &overlapped, &bytes, FALSE) || bytes == 0)
                {
                    continue;
                }
                size_t offset = 0;
                while (true)
                {
                    auto* information = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer + offset);
                    std::wstring name(information->FileName, information->FileNameLength / sizeof(WCHAR));
                    FileChange change;
                    change.path = m_implementation->root / name;
                    switch (information->Action)
                    {
                    case FILE_ACTION_ADDED:
                        change.type = FileChangeType::Created;
                        break;
                    case FILE_ACTION_REMOVED:
                        change.type = FileChangeType::Removed;
                        break;
                    case FILE_ACTION_RENAMED_OLD_NAME:
                    case FILE_ACTION_RENAMED_NEW_NAME:
                        change.type = FileChangeType::Renamed;
                        break;
                    default:
                        change.type = FileChangeType::Modified;
                        break;
                    }
                    Push(std::move(change));
                    if (information->NextEntryOffset == 0)
                    {
                        break;
                    }
                    offset += information->NextEntryOffset;
                }
            }
            CloseHandle(overlapped.hEvent);
        });
        return true;
    }

    void FileWatcher::Stop()
    {
        if (m_implementation->stopEvent)
        {
            SetEvent(m_implementation->stopEvent);
        }
        if (m_implementation->thread.joinable())
        {
            m_implementation->thread.join();
        }
        if (m_implementation->directory != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_implementation->directory);
            m_implementation->directory = INVALID_HANDLE_VALUE;
        }
        if (m_implementation->stopEvent)
        {
            CloseHandle(m_implementation->stopEvent);
            m_implementation->stopEvent = nullptr;
        }
    }

    bool FileWatcher::IsWatching() const
    {
        return m_implementation->directory != INVALID_HANDLE_VALUE;
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
