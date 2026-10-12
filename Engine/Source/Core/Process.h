#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace BrightBox3D::Core
{
    struct ProcessResult
    {
        int exitCode = -1;
        std::string output;
        bool started = false;
    };

    class Process
    {
    public:
        static ProcessResult Run(const std::vector<std::string>& arguments, const std::filesystem::path& workingDirectory = {});
    };

    class AsyncProcess
    {
    public:
        AsyncProcess() = default;
        ~AsyncProcess();

        AsyncProcess(const AsyncProcess&) = delete;
        AsyncProcess& operator=(const AsyncProcess&) = delete;

        void Start(std::vector<std::vector<std::string>> commands, const std::filesystem::path& workingDirectory = {});
        bool IsRunning() const { return m_running.load(); }
        bool HasResult() const { return m_finished.load(); }
        ProcessResult TakeResult();

    private:
        std::thread m_thread;
        std::atomic<bool> m_running{ false };
        std::atomic<bool> m_finished{ false };
        std::mutex m_mutex;
        ProcessResult m_result;
    };
}
