#include "Runtime/Project.h"

#include "Core/FileSystem.h"
#include "Core/SharedLibrary.h"
#include "Core/StringUtils.h"

namespace BrightBox3D::Runtime
{
    namespace
    {
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
        return GetBinariesDirectory() / Core::SharedLibrary::PlatformFileName(m_gameModuleName);
    }

    std::filesystem::path Project::GetStagingModulePath() const
    {
        return GetBinariesDirectory() / ProjectLayout::StagingFolder / Core::SharedLibrary::PlatformFileName(m_gameModuleName);
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

    bool Project::Create(const std::filesystem::path& directory, const std::string& name, std::string& error)
    {
        if (std::filesystem::exists(directory / FileName))
        {
            error = "A project already exists in this folder";
            return false;
        }
        if (!Core::FileSystem::EnsureDirectory(directory / ProjectLayout::AssetsFolder) || !Core::FileSystem::EnsureDirectory(directory / ProjectLayout::SourceFolder))
        {
            error = "Cannot create the project folders";
            return false;
        }
        Serialization::JsonDocument document;
        document.SetObject();
        auto& allocator = document.GetAllocator();
        document.AddMember("name", Serialization::MakeString(name, allocator), allocator);
        document.AddMember("engineVersion", "0.1.0", allocator);
        document.AddMember("gameModule", Serialization::MakeString(SanitizeModuleName(name), allocator), allocator);
        document.AddMember("startupLevel", rapidjson::Value(rapidjson::kNullType), allocator);
        Serialization::JsonValue settings(rapidjson::kObjectType);
        settings.AddMember("fixedTimeStep", 1.0 / 60.0, allocator);
        settings.AddMember("autoHotReload", true, allocator);
        document.AddMember("settings", settings, allocator);
        if (!Serialization::SaveJsonFile(directory / FileName, document))
        {
            error = "Cannot write the project file";
            return false;
        }
        Core::FileSystem::WriteText(directory / ProjectLayout::GitIgnoreFileName, "Cache/\nBuild/\nBinaries/\nCMakeLists.txt\n.DS_Store\n");
        return true;
    }

    bool Project::Open(const std::filesystem::path& directory, std::string& error)
    {
        Serialization::JsonDocument document;
        if (!Serialization::LoadJsonFile(directory / FileName, document, &error) || !document.IsObject())
        {
            if (error.empty())
            {
                error = "Invalid project file";
            }
            return false;
        }
        std::error_code ec;
        m_root = std::filesystem::weakly_canonical(directory, ec);
        if (ec)
        {
            m_root = directory;
        }
        m_name = Serialization::GetString(document, "name", m_root.filename().string());
        m_gameModuleName = Serialization::GetString(document, "gameModule", SanitizeModuleName(m_name));
        m_startupLevel = Core::Guid::Parse(Serialization::GetString(document, "startupLevel"));
        m_legacyGameMode.SetNull();
        if (const Serialization::JsonValue* gameMode = Serialization::FindMember(document, "gameMode"); gameMode && gameMode->IsObject())
        {
            m_legacyGameMode.CopyFrom(*gameMode, m_legacyGameMode.GetAllocator());
        }
        if (const Serialization::JsonValue* settings = Serialization::FindMember(document, "settings"))
        {
            m_fixedTimeStep = static_cast<float>(Serialization::GetNumber(*settings, "fixedTimeStep", 1.0 / 60.0));
            m_autoHotReload = Serialization::GetBool(*settings, "autoHotReload", true);
        }
        return true;
    }

    bool Project::Save() const
    {
        Serialization::JsonDocument document;
        document.SetObject();
        auto& allocator = document.GetAllocator();
        document.AddMember("name", Serialization::MakeString(m_name, allocator), allocator);
        document.AddMember("engineVersion", "0.1.0", allocator);
        document.AddMember("gameModule", Serialization::MakeString(m_gameModuleName, allocator), allocator);
        if (m_startupLevel.IsValid())
        {
            document.AddMember("startupLevel", Serialization::MakeString(m_startupLevel.ToString(), allocator), allocator);
        }
        else
        {
            document.AddMember("startupLevel", rapidjson::Value(rapidjson::kNullType), allocator);
        }
        Serialization::JsonValue settings(rapidjson::kObjectType);
        settings.AddMember("fixedTimeStep", static_cast<double>(m_fixedTimeStep), allocator);
        settings.AddMember("autoHotReload", m_autoHotReload, allocator);
        document.AddMember("settings", settings, allocator);
        return Serialization::SaveJsonFile(m_root / FileName, document);
    }
}
