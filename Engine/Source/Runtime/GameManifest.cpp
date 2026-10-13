#include "Runtime/GameManifest.h"

#include "Runtime/Project.h"
#include "Serialization/Json.h"
#include "Serialization/ObjectSerializer.h"

namespace BrightBox3D::Runtime
{
    void GameManifest::AssignFromProject(const Project& project)
    {
        name = project.GetName();
        gameModule = project.GetGameModuleName();
        startupLevel = project.GetStartupLevel();
        fixedTimeStep = project.GetFixedTimeStep();
    }

    bool GameManifest::Load(const std::filesystem::path& path, std::string& error)
    {
        Serialization::JsonDocument document;
        if (!Serialization::LoadJsonFile(path, document, &error) || !document.IsObject())
        {
            if (error.empty())
            {
                error = "invalid game manifest";
            }
            return false;
        }
        Serialization::DeserializeProperties(*this, document);
        return true;
    }

    bool GameManifest::Save(const std::filesystem::path& path) const
    {
        Serialization::JsonDocument document;
        document.SetObject();
        Serialization::SerializeProperties(*this, document, document.GetAllocator());
        return Serialization::SaveJsonFile(path, document);
    }
}
