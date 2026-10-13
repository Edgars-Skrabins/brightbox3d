#pragma once

#include "Core/Guid.h"
#include "Serialization/Json.h"

#include <filesystem>
#include <string>

namespace BrightBox3D::Runtime
{
    struct GameManifest
    {
        static constexpr const char* FileName = "Game.json";

        std::string name = "Game";
        std::string gameModule;
        Core::Guid startupLevel;
        Serialization::JsonDocument gameMode;
        float fixedTimeStep = 1.0f / 60.0f;

        bool Load(const std::filesystem::path& path, std::string& error);
        bool Save(const std::filesystem::path& path) const;
    };
}
