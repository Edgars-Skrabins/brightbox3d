#pragma once

#include "Core/Guid.h"
#include "Runtime/ProjectLayout.h"
#include "Serialization/Json.h"

#include <filesystem>
#include <string>

namespace BrightBox3D::Runtime
{
    class Project
    {
    public:
        static constexpr const char* FileName = ProjectLayout::ProjectFileName;

        static bool Create(const std::filesystem::path& directory, const std::string& name, std::string& error);
        static bool IsProjectDirectory(const std::filesystem::path& directory);

        bool Open(const std::filesystem::path& directory, std::string& error);
        bool Save() const;
        bool IsOpen() const { return !m_root.empty(); }

        const std::filesystem::path& GetRoot() const { return m_root; }
        std::filesystem::path GetAssetsDirectory() const { return m_root / ProjectLayout::AssetsFolder; }
        std::filesystem::path GetSourceDirectory() const { return m_root / ProjectLayout::SourceFolder; }
        std::filesystem::path GetCacheDirectory() const { return m_root / ProjectLayout::CacheFolder; }
        std::filesystem::path GetBinariesDirectory() const { return m_root / ProjectLayout::BinariesFolder; }
        std::filesystem::path GetGameModulePath() const;
        std::filesystem::path GetStagingModulePath() const;
        std::filesystem::path GetBuildDirectory() const { return m_root / ProjectLayout::BuildFolder; }
        std::filesystem::path GetCompileDirectory() const { return GetCacheDirectory() / ProjectLayout::CompileCacheFolder; }
        std::filesystem::path GetHotReloadCacheDirectory() const { return GetCacheDirectory() / ProjectLayout::HotReloadCacheFolder; }
        const std::string& GetName() const { return m_name; }

        const Core::Guid& GetStartupLevel() const { return m_startupLevel; }
        void SetStartupLevel(const Core::Guid& level) { m_startupLevel = level; }
        const Serialization::JsonValue* GetLegacyGameMode() const { return m_legacyGameMode.IsObject() ? &m_legacyGameMode : nullptr; }
        void ClearLegacyGameMode() { m_legacyGameMode.SetNull(); }
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
        Serialization::JsonDocument m_legacyGameMode;
        float m_fixedTimeStep = 1.0f / 60.0f;
        bool m_autoHotReload = true;
    };
}
