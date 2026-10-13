#include "Runtime/Project.h"

#include "Core/EngineInfo.h"
#include "Core/EngineText.h"
#include "Core/FileSystem.h"
#include "Core/SharedLibrary.h"
#include "Core/StringUtils.h"
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
                result = "Game" + result;
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

    bool Project::Create(const std::filesystem::path& directory, const std::string& name, std::string& error)
    {
        if (std::filesystem::exists(directory / FileName))
        {
            error = Core::Text(Core::EngineText::ProjectExists);
            return false;
        }
        if (!Core::FileSystem::EnsureDirectory(directory / ProjectLayout::AssetsFolder) || !Core::FileSystem::EnsureDirectory(directory / ProjectLayout::SourceFolder))
        {
            error = Core::Text(Core::EngineText::ProjectFoldersFailed);
            return false;
        }
        ProjectSettings settings;
        settings.name = name;
        settings.gameModule = SanitizeModuleName(name);
        if (!WriteSettings(directory / FileName, settings))
        {
            error = Core::Text(Core::EngineText::ProjectWriteFailed);
            return false;
        }
        if (std::optional<std::string> gitIgnore = ProjectTemplates::GitIgnore())
        {
            Core::FileSystem::WriteText(directory / ProjectLayout::GitIgnoreFileName, *gitIgnore);
        }
        return true;
    }

    bool Project::Open(const std::filesystem::path& directory, std::string& error)
    {
        Serialization::JsonDocument document;
        if (!Serialization::LoadJsonFile(directory / FileName, document, &error) || !document.IsObject())
        {
            if (error.empty())
            {
                error = Core::Text(Core::EngineText::ProjectInvalid);
            }
            return false;
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
        return true;
    }

    bool Project::Save() const
    {
        return WriteSettings(m_root / FileName, m_settings);
    }
}
