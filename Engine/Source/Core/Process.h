#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
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
        bool terminated = false;
    };

    class ProcessHandle
    {
    public:
        ProcessHandle() = default;
        ProcessHandle(const ProcessHandle&) = delete;
        ProcessHandle& operator=(const ProcessHandle&) = delete;

        void Terminate();
        void Kill();
        bool IsTerminated() const { return m_terminated.load(); }

    private:
        friend class Process;

        bool Attach(std::intptr_t native);
        void Detach();

        std::mutex m_mutex;
        std::atomic<bool> m_terminated{ false };
        std::intptr_t m_native = 0;
    };

    class Process
    {
    public:
        static ProcessResult Run(const std::vector<std::string>& arguments, const std::filesystem::path& workingDirectory = {}, ProcessHandle* handle = nullptr);
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
        bool Terminate(std::chrono::milliseconds timeout);

    private:
        std::unique_ptr<ProcessHandle> m_handle;
        std::thread m_thread;
        std::atomic<bool> m_running{ false };
        std::atomic<bool> m_finished{ false };
        std::mutex m_mutex;
        ProcessResult m_result;
    };
}
