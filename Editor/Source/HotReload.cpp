#include "HotReload.h"

#include "Core/EngineInfo.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/StringUtils.h"
#include "EditorContext.h"
#include "EditorText.h"

#include <regex>
#include <sstream>
#include <thread>

namespace BrightBox3D::Editor
{
    namespace
    {
        std::filesystem::path SdkPath()
        {
            return Core::FileSystem::ExecutableDirectory() / "BrightBox3DSDK.cmake";
        }

        constexpr const char* NinjaGenerator = "Ninja";
        constexpr std::chrono::milliseconds CancelTimeout(2000);
        constexpr const char* GeneratorCacheEntry = "CMAKE_GENERATOR:INTERNAL=";

        std::string CachedGenerator(const std::filesystem::path& buildDirectory)
        {
            auto cache = Core::FileSystem::ReadText(buildDirectory / "CMakeCache.txt");
            if (!cache)
            {
                return {};
            }
            for (const std::string& line : Core::Split(*cache, '\n'))
            {
                if (Core::StartsWith(line, GeneratorCacheEntry))
                {
                    return Core::Trim(line.substr(std::string(GeneratorCacheEntry).size()));
                }
            }
            return {};
        }

        std::string SharedLibraryName(const std::string& name)
        {
            return Core::SharedLibrary::PlatformFileName(name);
        }
    }

    HotReload::HotReload(EditorContext& context) : m_context(context)
    {
    }

    HotReload::~HotReload()
    {
        Cancel();
    }

    std::filesystem::path HotReload::FindCMake()
    {
        std::vector<std::filesystem::path> candidates;
        if (const char* path = std::getenv("PATH"))
        {
#if defined(_WIN32)
            char separator = ';';
            const char* executable = "cmake.exe";
#else
            char separator = ':';
            const char* executable = "cmake";
#endif
            for (const std::string& entry : Core::Split(path, separator))
            {
                if (!entry.empty())
                {
                    candidates.push_back(std::filesystem::path(entry) / executable);
                }
            }
        }
        candidates.push_back("/opt/homebrew/bin/cmake");
        candidates.push_back("/usr/local/bin/cmake");
        candidates.push_back("/Applications/CMake.app/Contents/bin/cmake");
        candidates.push_back("C:/Program Files/CMake/bin/cmake.exe");
        for (const std::filesystem::path& candidate : candidates)
        {
            std::error_code error;
            if (std::filesystem::is_regular_file(candidate, error))
            {
                return candidate;
            }
        }
        return {};
    }

    std::filesystem::path HotReload::FindNinja()
    {
#if defined(_WIN32)
        std::filesystem::path ninja = Core::FileSystem::ExecutableDirectory() / "ninja.exe";
#else
        std::filesystem::path ninja = Core::FileSystem::ExecutableDirectory() / "ninja";
#endif
        std::error_code error;
        return std::filesystem::is_regular_file(ninja, error) ? ninja : std::filesystem::path();
    }

    std::filesystem::path HotReload::GetModulePath() const
    {
        const Runtime::Project& project = m_context.GetProjectService().GetProject();
        return project.GetRoot() / "Binaries" / SharedLibraryName(project.GetGameModuleName());
    }

    std::filesystem::path HotReload::GetStagingModulePath() const
    {
        std::filesystem::path module = GetModulePath();
        return module.parent_path() / "Staging" / module.filename();
    }

    bool HotReload::PromoteStagedModule(std::string& error)
    {
        std::filesystem::path staged = GetStagingModulePath();
        std::error_code code;
        if (!std::filesystem::exists(staged, code))
        {
            error = Text(EditorText::NoModuleProduced, staged.generic_string());
            return false;
        }
        std::filesystem::path module = GetModulePath();
        std::filesystem::copy_file(staged, module, std::filesystem::copy_options::overwrite_existing, code);
        if (code)
        {
            error = code.message();
            return false;
        }
        return true;
    }

    bool HotReload::HasSources() const
    {
        const Runtime::Project& project = m_context.GetProjectService().GetProject();
        std::error_code error;
        for (std::filesystem::recursive_directory_iterator it(project.GetSourceDirectory(), error), end; !error && it != end; it.increment(error))
        {
            std::string extension = it->path().extension().string();
            if (extension == ".cpp" || extension == ".h")
            {
                return true;
            }
        }
        return false;
    }

    bool HotReload::GenerateProjectFiles()
    {
        Runtime::Project& project = m_context.GetProjectService().GetProject();
        std::filesystem::path root = project.GetRoot();
        std::filesystem::path cmakeLists = root / "CMakeLists.txt";
        std::string name = project.GetGameModuleName();
        std::ostringstream content;
        content << "cmake_minimum_required(VERSION 3.24)\n";
        content << "project(" << name << " LANGUAGES C CXX)\n\n";
        content << "set(BRIGHT_SDK \"" << Core::FileSystem::ToGenericString(SdkPath()) << "\" CACHE FILEPATH \"\")\n";
        content << "include(${BRIGHT_SDK})\n";
        content << "include(${BRIGHT_ENGINE_SOURCE_DIR}/cmake/BrightGame.cmake)\n\n";
        content << "bright_add_game_module(" << name << " ${CMAKE_CURRENT_SOURCE_DIR}/Source ${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Staging)\n";
        auto existing = Core::FileSystem::ReadText(cmakeLists);
        if (!existing || *existing != content.str())
        {
            if (existing && existing->find("bright_add_game_module") == std::string::npos)
            {
                Core::Log::Warning(Text(EditorText::CMakeListsNotGenerated, Core::EngineName));
                return true;
            }
            if (!Core::FileSystem::WriteText(cmakeLists, content.str()))
            {
                return false;
            }
        }
        return true;
    }

    void HotReload::OnProjectOpened()
    {
        GenerateProjectFiles();
        std::filesystem::path module = GetModulePath();
        if (std::filesystem::exists(module))
        {
            std::string error;
            if (m_module.Load(module, m_context.GetProjectService().GetProject().GetGameModuleName(), m_context.GetProjectService().GetProject().GetCacheDirectory() / "HotReload", error))
            {
                m_statusText = Text(EditorText::GameCodeLoaded);
            }
            else
            {
                Core::Log::Error(Text(EditorText::GameModuleLoadFailed, error));
                m_statusText = Text(EditorText::GameCodeLoadFailed);
            }
        }
        if (HasSources())
        {
            RequestCompile();
        }
    }

    void HotReload::OnProjectClosed()
    {
        Cancel();
        if (m_process.HasResult())
        {
            m_process.TakeResult();
        }
        m_module.Unload();
        m_state = CompileState::Idle;
        m_statusText = Text(EditorText::NoGameCode);
    }

    void HotReload::NotifySourceChanged(const std::filesystem::path& path)
    {
        std::string extension = Core::ToLower(path.extension().string());
        if (extension != ".cpp" && extension != ".h" && extension != ".hpp" && extension != ".inl")
        {
            return;
        }
        m_pendingChange = true;
        m_changeTime = std::chrono::steady_clock::now();
    }

    void HotReload::RequestCompile()
    {
        m_compileRequested = true;
    }

    void HotReload::Cancel()
    {
        m_compileRequested = false;
        if (m_state != CompileState::Compiling)
        {
            return;
        }
        m_process.Terminate(CancelTimeout);
        if (m_process.HasResult())
        {
            m_process.TakeResult();
        }
        m_state = CompileState::Idle;
        m_statusText = Text(EditorText::CompileCancelled);
        Core::Log::Write(Core::LogLevel::Info, Text(EditorText::CompileCancelled), "Compiler");
    }

    void HotReload::Update()
    {
        if (!m_context.GetProjectService().HasProject())
        {
            return;
        }
        if (m_pendingChange && std::chrono::steady_clock::now() - m_changeTime > std::chrono::milliseconds(350))
        {
            m_pendingChange = false;
            if (m_context.GetProjectService().GetProject().GetAutoHotReload())
            {
                m_compileRequested = true;
            }
            else
            {
                m_statusText = Text(EditorText::SourceChanged);
            }
        }
        if (m_state == CompileState::Compiling && m_process.HasResult())
        {
            FinishCompile();
        }
        if (m_compileRequested && m_state != CompileState::Compiling)
        {
            m_compileRequested = false;
            StartCompile();
        }
    }

    void HotReload::StartCompile()
    {
        if (!HasSources())
        {
            m_statusText = Text(EditorText::NoGameCode);
            return;
        }
        std::filesystem::path cmake = FindCMake();
        if (cmake.empty())
        {
            Core::Log::Error(Text(EditorText::CMakeNotFoundError));
            m_state = CompileState::Failed;
            m_statusText = Text(EditorText::CMakeNotFound);
            return;
        }
        if (!GenerateProjectFiles())
        {
            Core::Log::Error(Text(EditorText::CMakeListsWriteFailed));
            return;
        }
        Runtime::Project& project = m_context.GetProjectService().GetProject();
        std::filesystem::path buildDirectory = project.GetCacheDirectory() / "Build";
        std::filesystem::path ninja = FindNinja();
        if (!ninja.empty() && std::filesystem::exists(buildDirectory / "CMakeCache.txt") && CachedGenerator(buildDirectory) != NinjaGenerator)
        {
            std::error_code error;
            std::filesystem::remove_all(buildDirectory, error);
        }
        std::vector<std::vector<std::string>> commands;
        if (!std::filesystem::exists(buildDirectory / "CMakeCache.txt"))
        {
            std::vector<std::string> configure = { cmake.string(), "-S", project.GetRoot().string(), "-B", buildDirectory.string(), "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DBRIGHT_SDK=" + Core::FileSystem::ToGenericString(SdkPath()) };
            if (!ninja.empty())
            {
                configure.insert(configure.end(), { "-G", NinjaGenerator, "-DCMAKE_MAKE_PROGRAM=" + Core::FileSystem::ToGenericString(ninja) });
            }
            commands.push_back(configure);
        }
        unsigned int jobs = std::max(1u, std::thread::hardware_concurrency());
        commands.push_back({ cmake.string(), "--build", buildDirectory.string(), "--target", project.GetGameModuleName(), "--config", "RelWithDebInfo", "-j", std::to_string(jobs) });
        m_process.Start(commands, project.GetRoot());
        m_state = CompileState::Compiling;
        m_statusText = Text(EditorText::Compiling);
        m_compileStart = std::chrono::steady_clock::now();
        Core::Log::Write(Core::LogLevel::Info, Text(EditorText::CompilingGameCode), "Compiler");
    }

    void HotReload::ReportCompilerOutput(const std::string& output)
    {
        static const std::regex clangPattern(R"(^(.+?):(\d+):(?:(\d+):)? (fatal error|error|warning): (.*)$)");
        static const std::regex msvcPattern(R"(^\s*(.+?)\((\d+)(?:,\d+)?\)\s*: (fatal error|error|warning) ([A-Z]+\d+): (.*)$)");
        std::istringstream stream(output);
        std::string line;
        int errors = 0;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            std::smatch match;
            if (std::regex_match(line, match, clangPattern))
            {
                bool isError = match[4].str() != "warning";
                errors += isError ? 1 : 0;
                Core::Log::Write(isError ? Core::LogLevel::Error : Core::LogLevel::Warning, match[5].str(), "Compiler", match[1].str(), std::stoi(match[2].str()));
            }
            else if (std::regex_match(line, match, msvcPattern))
            {
                bool isError = match[3].str() != "warning";
                errors += isError ? 1 : 0;
                Core::Log::Write(isError ? Core::LogLevel::Error : Core::LogLevel::Warning, match[4].str() + ": " + match[5].str(), "Compiler", match[1].str(), std::stoi(match[2].str()));
            }
        }
        if (errors == 0 && m_state == CompileState::Failed)
        {
            Core::Log::Write(Core::LogLevel::Error, output, "Compiler");
        }
    }

    void HotReload::FinishCompile()
    {
        Core::ProcessResult result = m_process.TakeResult();
        if (result.terminated)
        {
            m_state = CompileState::Idle;
            m_statusText = Text(EditorText::CompileCancelled);
            return;
        }
        double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_compileStart).count();
        if (!result.started || result.exitCode != 0)
        {
            m_state = CompileState::Failed;
            m_statusText = Text(EditorText::CompileFailed);
            ReportCompilerOutput(result.output);
            Core::Log::Write(Core::LogLevel::Error, Text(EditorText::GameCodeCompileFailed), "Compiler");
            if (OnCompileFailed)
            {
                OnCompileFailed();
            }
            return;
        }
        ReportCompilerOutput(result.output);
        std::filesystem::path module = GetModulePath();
        std::string error;
        if (!PromoteStagedModule(error))
        {
            m_state = CompileState::Failed;
            m_statusText = Text(EditorText::ReloadFailed);
            Core::Log::Error(Text(EditorText::ModulePromoteFailed, error));
            if (OnCompileFailed)
            {
                OnCompileFailed();
            }
            return;
        }
        bool loaded = m_module.IsLoaded() ? m_module.Reload(module, error)
                                          : m_module.Load(module, m_context.GetProjectService().GetProject().GetGameModuleName(), m_context.GetProjectService().GetProject().GetCacheDirectory() / "HotReload", error);
        if (!loaded)
        {
            m_state = CompileState::Failed;
            m_statusText = Text(EditorText::ReloadFailed);
            Core::Log::Error(Text(EditorText::GameModuleReloadFailed, error));
            if (OnCompileFailed)
            {
                OnCompileFailed();
            }
            return;
        }
        m_state = CompileState::Succeeded;
        m_statusText = Text(EditorText::Reloaded, seconds);
        if (OnReloaded)
        {
            OnReloaded();
        }
    }
}
