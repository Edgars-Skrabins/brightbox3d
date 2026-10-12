#include "Core/Process.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace BrightBox3D::Core
{
#if defined(_WIN32)
    namespace
    {
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

    ProcessResult Process::Run(const std::vector<std::string>& arguments, const std::filesystem::path& workingDirectory)
    {
        ProcessResult result;
        if (arguments.empty())
        {
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

        BOOL created = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, directory.empty() ? nullptr : directory.c_str(), &startup, &information);
        CloseHandle(writePipe);
        if (!created)
        {
            CloseHandle(readPipe);
            result.output = "Failed to start process: " + arguments.front();
            return result;
        }
        result.started = true;

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
        CloseHandle(information.hProcess);
        CloseHandle(information.hThread);
        return result;
    }
#else
    ProcessResult Process::Run(const std::vector<std::string>& arguments, const std::filesystem::path& workingDirectory)
    {
        ProcessResult result;
        if (arguments.empty())
        {
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

        pid_t pid = 0;
        int status = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        close(pipeDescriptors[1]);
        if (status != 0)
        {
            close(pipeDescriptors[0]);
            result.output = "Failed to start process: " + arguments.front();
            return result;
        }
        result.started = true;

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
        return result;
    }
#endif

    AsyncProcess::~AsyncProcess()
    {
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
        m_running = true;
        m_finished = false;
        m_thread = std::thread([this, commands = std::move(commands), workingDirectory]
        {
            ProcessResult combined;
            combined.started = true;
            combined.exitCode = 0;
            for (const std::vector<std::string>& command : commands)
            {
                ProcessResult result = Process::Run(command, workingDirectory);
                combined.output += result.output;
                if (!result.started || result.exitCode != 0)
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
