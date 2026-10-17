#pragma once

#include "Core/Guid.h"
#include "Core/Result.h"
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
        static constexpr const char* DefaultName = "Game";

        BrightProperty()
        std::string name = DefaultName;

        BrightProperty()
        std::string gameModule;

        BrightProperty()
        Core::Guid startupLevel;

        BrightProperty()
        float fixedTimeStep = 1.0f / 60.0f;

        void AssignFromProject(const Project& project);
        Core::Result<void> Load(const std::filesystem::path& path);
        bool Save(const std::filesystem::path& path) const;
    };
}
