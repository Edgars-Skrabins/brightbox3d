#pragma once

#include "Core/Guid.h"
#include "Serialization/Json.h"

#include <filesystem>
#include <string>

namespace BrightBox3D::Runtime
{
    class Project
    {
    public:
        static constexpr const char* FileName = "Project.bbproject";

        static bool Create(const std::filesystem::path& directory, const std::string& name, std::string& error);
        static bool IsProjectDirectory(const std::filesystem::path& directory);

        bool Open(const std::filesystem::path& directory, std::string& error);
        bool Save() const;
        bool IsOpen() const { return !m_root.empty(); }

        const std::filesystem::path& GetRoot() const { return m_root; }
        std::filesystem::path GetAssetsDirectory() const { return m_root / "Assets"; }
        std::filesystem::path GetSourceDirectory() const { return m_root / "Source"; }
        std::filesystem::path GetCacheDirectory() const { return m_root / "Cache"; }
        const std::string& GetName() const { return m_name; }

        const Core::Guid& GetStartupLevel() const { return m_startupLevel; }
        void SetStartupLevel(const Core::Guid& level) { m_startupLevel = level; }
        Serialization::JsonDocument& GetGameMode() { return m_gameMode; }
        const Serialization::JsonDocument& GetGameMode() const { return m_gameMode; }
        float GetFixedTimeStep() const { return m_fixedTimeStep; }
        void SetFixedTimeStep(float step) { m_fixedTimeStep = step; }
        bool GetAutoHotReload() const { return m_autoHotReload; }
        void SetAutoHotReload(bool enabled) { m_autoHotReload = enabled; }
        const std::string& GetGameModuleName() const { return m_gameModuleName; }

        static Project* GetCurrent();
        static void SetCurrent(Project* project);

    private:
        std::filesystem::path m_root;
        std::string m_name;
        std::string m_gameModuleName = "Game";
        Core::Guid m_startupLevel;
        Serialization::JsonDocument m_gameMode;
        float m_fixedTimeStep = 1.0f / 60.0f;
        bool m_autoHotReload = true;
    };
}
