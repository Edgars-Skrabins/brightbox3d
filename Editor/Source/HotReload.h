#pragma once

#include "Core/FileWatcher.h"
#include "Core/Process.h"
#include "Runtime/GameModule.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Editor
{
    class EditorContext;

    enum class CompileState
    {
        Idle,
        Compiling,
        Succeeded,
        Failed
    };

    class HotReload
    {
    public:
        explicit HotReload(EditorContext& context);
        ~HotReload();

        void OnProjectOpened();
        void OnProjectClosed();
        void Update();
        void RequestCompile();
        void NotifySourceChanged(const std::filesystem::path& path);

        CompileState GetState() const { return m_state; }
        bool IsCompiling() const { return m_state == CompileState::Compiling; }
        const std::string& GetStatusText() const { return m_statusText; }
        Runtime::GameModule& GetModule() { return m_module; }
        std::filesystem::path GetModulePath() const;
        static std::filesystem::path FindCMake();

        std::function<std::vector<Level::Level*>()> GetLevels;
        std::function<void()> OnCompileFailed;
        std::function<void()> OnReloaded;

    private:
        bool GenerateProjectFiles();
        void StartCompile();
        void FinishCompile();
        void ReportCompilerOutput(const std::string& output);
        bool HasSources() const;

        EditorContext& m_context;
        Core::AsyncProcess m_process;
        Runtime::GameModule m_module;
        CompileState m_state = CompileState::Idle;
        std::string m_statusText = "No game code";
        bool m_compileRequested = false;
        bool m_pendingChange = false;
        std::chrono::steady_clock::time_point m_changeTime;
        std::chrono::steady_clock::time_point m_compileStart;
    };
}
