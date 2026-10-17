#include "Runtime/Project.h"

#include "Core/EngineInfo.h"
#include "Core/EngineText.h"
#include "Core/FileSystem.h"
#include "Core/SharedLibrary.h"
#include "Core/StringUtils.h"
#include "Runtime/GameManifest.h"
#include "Runtime/ProjectTemplates.h"
#include "Serialization/ObjectSerializer.h"

namespace BrightBox3D::Runtime
{
    namespace
    {
        constexpr const char* LegacySettingsKey = "settings";
        constexpr const char* LegacyGameModeKey = "gameMode";

        Project* s_current = nullptr;

        std::string SanitizeModuleName(const std::string& name)
        {
            std::string result;
            for (char c : name)
            {
                if (std::isalnum(static_cast<unsigned char>(c)))
                {
                    result += c;
                }
            }
            if (result.empty() || std::isdigit(static_cast<unsigned char>(result[0])))
            {
                result = GameManifest::DefaultName + result;
            }
            return result;
        }
    }

    std::filesystem::path Project::GetGameModulePath() const
    {
        return GetBinariesDirectory() / Core::SharedLibrary::PlatformFileName(m_settings.gameModule);
    }

    std::filesystem::path Project::GetStagingModulePath() const
    {
        return GetBinariesDirectory() / ProjectLayout::StagingFolder / Core::SharedLibrary::PlatformFileName(m_settings.gameModule);
    }

    Project* Project::GetCurrent()
    {
        return s_current;
    }

    void Project::SetCurrent(Project* project)
    {
        s_current = project;
    }

    bool Project::IsProjectDirectory(const std::filesystem::path& directory)
    {
        return std::filesystem::exists(directory / FileName);
    }

    bool Project::WriteSettings(const std::filesystem::path& path, const ProjectSettings& settings)
    {
        ProjectSettings stamped = settings;
        stamped.engineVersion = Core::EngineVersion;
        Serialization::JsonDocument document;
        document.SetObject();
        Serialization::SerializeProperties(stamped, document, document.GetAllocator());
        return Serialization::SaveJsonFile(path, document);
    }

    Core::Result<void> Project::Create(const std::filesystem::path& directory, const std::string& name)
    {
        if (std::filesystem::exists(directory / FileName))
        {
            return Core::Error(Core::EngineText::ProjectExists);
        }
        if (!Core::FileSystem::EnsureDirectory(directory / ProjectLayout::AssetsFolder) || !Core::FileSystem::EnsureDirectory(directory / ProjectLayout::SourceFolder))
        {
            return Core::Error(Core::EngineText::ProjectFoldersFailed);
        }
        ProjectSettings settings;
        settings.name = name;
        settings.gameModule = SanitizeModuleName(name);
        if (!WriteSettings(directory / FileName, settings))
        {
            return Core::Error(Core::EngineText::ProjectWriteFailed);
        }
        if (std::optional<std::string> gitIgnore = ProjectTemplates::GitIgnore())
        {
            Core::FileSystem::WriteText(directory / ProjectLayout::GitIgnoreFileName, *gitIgnore);
        }
        return {};
    }

    Core::Result<void> Project::Open(const std::filesystem::path& directory)
    {
        Serialization::JsonDocument document;
        if (Core::Result<void> loaded = Serialization::LoadJsonFile(directory / FileName, document); !loaded)
        {
            return loaded;
        }
        if (!document.IsObject())
        {
            return Core::Error(Core::EngineText::ProjectInvalid);
        }
        std::error_code ec;
        m_root = std::filesystem::weakly_canonical(directory, ec);
        if (ec)
        {
            m_root = directory;
        }
        m_settings = ProjectSettings();
        Serialization::DeserializeProperties(m_settings, document);
        if (const Serialization::JsonValue* legacySettings = Serialization::FindMember(document, LegacySettingsKey))
        {
            Serialization::DeserializeProperties(m_settings, *legacySettings);
        }
        if (m_settings.name.empty())
        {
            m_settings.name = m_root.filename().string();
        }
        if (m_settings.gameModule.empty())
        {
            m_settings.gameModule = SanitizeModuleName(m_settings.name);
        }
        m_legacyGameMode.SetNull();
        if (const Serialization::JsonValue* gameMode = Serialization::FindMember(document, LegacyGameModeKey); gameMode && gameMode->IsObject())
        {
            m_legacyGameMode.CopyFrom(*gameMode, m_legacyGameMode.GetAllocator());
        }
        return {};
    }

    bool Project::Save() const
    {
        return WriteSettings(m_root / FileName, m_settings);
    }
}
