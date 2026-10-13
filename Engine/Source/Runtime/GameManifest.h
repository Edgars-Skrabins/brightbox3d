#pragma once

#include "Core/Guid.h"
#include "Reflection/EngineObject.h"

#include <filesystem>
#include <string>

namespace BrightBox3D::Runtime
{
    class Project;

    BrightClass()
    class GameManifest : public Reflection::EngineObject
    {
        BrightBody()
    public:
        static constexpr const char* FileName = "Game.json";

        BrightProperty()
        std::string name = "Game";

        BrightProperty()
        std::string gameModule;

        BrightProperty()
        Core::Guid startupLevel;

        BrightProperty()
        float fixedTimeStep = 1.0f / 60.0f;

        void AssignFromProject(const Project& project);
        bool Load(const std::filesystem::path& path, std::string& error);
        bool Save(const std::filesystem::path& path) const;
    };
}
