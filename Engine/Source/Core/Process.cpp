#include "Core/Process.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace BrightBox3D::Core
{
    bool ProcessHandle::Attach(std::intptr_t native)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_native = native;
        return !m_terminated.load();
    }

    void ProcessHandle::Detach()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_native = 0;
    }

#if defined(_WIN32)
    void ProcessHandle::Terminate()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_terminated = true;
        if (m_native != 0)
        {
            TerminateJobObject(reinterpret_cast<HANDLE>(m_native), 1);
        }
    }

    void ProcessHandle::Kill()
    {
        Terminate();
    }

    namespace
    {
        HANDLE CreateKillOnCloseJob()
        {
            HANDLE job = CreateJobObjectW(nullptr, nullptr);
            if (!job)
            {
                return nullptr;
            }
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
            return job;
        }

        std::wstring Widen(const std::string& text)
        {
            if (text.empty())
            {
                return {};
            }
            int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
            std::wstring result(static_cast<size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
            return result;
        }

        std::wstring QuoteArgument(const std::string& argument)
        {
            std::wstring wide = Widen(argument);
            if (!wide.empty() && wide.find_first_of(L" \t\"") == std::wstring::npos)
            {
                return wide;
            }
            std::wstring quoted = L"\"";
            for (wchar_t c : wide)
            {
                if (c == L'"')
                {
                    quoted += L'\\';
                }
                quoted += c;
            }
            quoted += L"\"";
            return quoted;
        }
    }

    ProcessResult Process::Run(const std::vector<std::string>& arguments, const std::filesystem::path& workingDirectory, ProcessHandle* handle)
    {
        ProcessResult result;
        if (arguments.empty() || (handle && handle->IsTerminated()))
        {
            result.terminated = handle && handle->IsTerminated();
            return result;
        }

        SECURITY_ATTRIBUTES attributes{};
        attributes.nLength = sizeof(attributes);
        attributes.bInheritHandle = TRUE;
        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;
        if (!CreatePipe(&readPipe, &writePipe, &attributes, 0))
        {
            return result;
        }
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        std::wstring commandLine;
        for (const std::string& argument : arguments)
        {
            if (!commandLine.empty())
            {
                commandLine += L' ';
            }
            commandLine += QuoteArgument(argument);
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = writePipe;
        startup.hStdError = writePipe;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION information{};
        std::wstring directory = workingDirectory.empty() ? std::wstring() : workingDirectory.wstring();

        BOOL created = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, directory.empty() ? nullptr : directory.c_str(), &startup, &information);
        CloseHandle(writePipe);
        if (!created)
        {
            CloseHandle(readPipe);
            result.output = "Failed to start process: " + arguments.front();
            return result;
        }
        result.started = true;
        HANDLE job = CreateKillOnCloseJob();
        if (job)
        {
            AssignProcessToJobObject(job, information.hProcess);
            if (handle && !handle->Attach(reinterpret_cast<std::intptr_t>(job)))
            {
                TerminateJobObject(job, 1);
            }
        }
        ResumeThread(information.hThread);

        char buffer[4096];
        DWORD bytesRead = 0;
        while (ReadFile(readPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
        {
            result.output.append(buffer, bytesRead);
        }
        CloseHandle(readPipe);
        WaitForSingleObject(information.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(information.hProcess, &exitCode);
        result.exitCode = static_cast<int>(exitCode);
        if (handle)
        {
            handle->Detach();
            result.terminated = handle->IsTerminated();
        }
        if (job)
        {
            CloseHandle(job);
        }
        CloseHandle(information.hProcess);
        CloseHandle(information.hThread);
        return result;
    }
#else
    void ProcessHandle::Terminate()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_terminated = true;
        if (m_native > 0)
        {
            killpg(static_cast<pid_t>(m_native), SIGTERM);
        }
    }

    void ProcessHandle::Kill()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_terminated = true;
        if (m_native > 0)
        {
            killpg(static_cast<pid_t>(m_native), SIGKILL);
        }
    }

    ProcessResult Process::Run(const std::vector<std::string>& arguments, const std::filesystem::path& workingDirectory, ProcessHandle* handle)
    {
        ProcessResult result;
        if (arguments.empty() || (handle && handle->IsTerminated()))
        {
            result.terminated = handle && handle->IsTerminated();
            return result;
        }

        int pipeDescriptors[2];
        if (pipe(pipeDescriptors) != 0)
        {
            return result;
        }

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addclose(&actions, pipeDescriptors[0]);
        posix_spawn_file_actions_adddup2(&actions, pipeDescriptors[1], STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, pipeDescriptors[1], STDERR_FILENO);
        posix_spawn_file_actions_addclose(&actions, pipeDescriptors[1]);
        if (!workingDirectory.empty())
        {
#if defined(__APPLE__) && defined(__MAC_26_0)
            posix_spawn_file_actions_addchdir(&actions, workingDirectory.c_str());
#else
            posix_spawn_file_actions_addchdir_np(&actions, workingDirectory.c_str());
#endif
        }

        std::vector<char*> argv;
        for (const std::string& argument : arguments)
        {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }
        argv.push_back(nullptr);

        posix_spawnattr_t attributes;
        posix_spawnattr_init(&attributes);
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
        posix_spawnattr_setpgroup(&attributes, 0);

        pid_t pid = 0;
        int status = posix_spawnp(&pid, argv[0], &actions, &attributes, argv.data(), environ);
        posix_spawnattr_destroy(&attributes);
        posix_spawn_file_actions_destroy(&actions);
        close(pipeDescriptors[1]);
        if (status != 0)
        {
            close(pipeDescriptors[0]);
            result.output = "Failed to start process: " + arguments.front();
            return result;
        }
        result.started = true;
        if (handle && !handle->Attach(pid))
        {
            killpg(pid, SIGTERM);
        }

        char buffer[4096];
        while (true)
        {
            ssize_t count = read(pipeDescriptors[0], buffer, sizeof(buffer));
            if (count > 0)
            {
                result.output.append(buffer, static_cast<size_t>(count));
            }
            else if (count == 0 || errno != EINTR)
            {
                break;
            }
        }
        close(pipeDescriptors[0]);

        int waitStatus = 0;
        while (waitpid(pid, &waitStatus, 0) < 0 && errno == EINTR)
        {
        }
        result.exitCode = WIFEXITED(waitStatus) ? WEXITSTATUS(waitStatus) : -1;
        if (handle)
        {
            handle->Detach();
            result.terminated = handle->IsTerminated();
        }
        return result;
    }
#endif

    AsyncProcess::~AsyncProcess()
    {
        Terminate(std::chrono::seconds(2));
        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    void AsyncProcess::Start(std::vector<std::vector<std::string>> commands, const std::filesystem::path& workingDirectory)
    {
        if (m_thread.joinable())
        {
            m_thread.join();
        }
        m_handle = std::make_unique<ProcessHandle>();
        m_running = true;
        m_finished = false;
        m_thread = std::thread([this, commands = std::move(commands), workingDirectory]
        {
            ProcessResult combined;
            combined.started = true;
            combined.exitCode = 0;
            for (const std::vector<std::string>& command : commands)
            {
                ProcessResult result = Process::Run(command, workingDirectory, m_handle.get());
                combined.output += result.output;
                combined.terminated = result.terminated;
                if (result.terminated || !result.started || result.exitCode != 0)
                {
                    combined.started = result.started;
                    combined.exitCode = result.exitCode == 0 ? -1 : result.exitCode;
                    break;
                }
            }
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_result = std::move(combined);
            }
            m_running = false;
            m_finished = true;
        });
    }

    bool AsyncProcess::Terminate(std::chrono::milliseconds timeout)
    {
        if (!m_running || !m_handle)
        {
            return true;
        }
        m_handle->Terminate();
        auto waitUntilStopped = [this](std::chrono::milliseconds limit)
        {
            auto deadline = std::chrono::steady_clock::now() + limit;
            while (m_running && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return !m_running;
        };
        if (!waitUntilStopped(timeout))
        {
            m_handle->Kill();
            waitUntilStopped(timeout);
        }
        if (!m_running && m_thread.joinable())
        {
            m_thread.join();
        }
        return !m_running;
    }

    ProcessResult AsyncProcess::TakeResult()
    {
        if (m_thread.joinable() && !m_running)
        {
            m_thread.join();
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_finished = false;
        return std::move(m_result);
    }
}
