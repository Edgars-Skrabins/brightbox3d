#pragma once

#include "Core/CommandLine.h"

#include <filesystem>

namespace BrightBox3D::Player
{
    struct PlayerOptions
    {
        std::filesystem::path project;
        int frames = -1;
        std::filesystem::path screenshot;
    };

    inline const Core::CommandLineOption<PlayerOptions> PlayerCommandLine[] = {
        { "--project", &PlayerOptions::project },
        { "--frames", &PlayerOptions::frames },
        { "--screenshot", &PlayerOptions::screenshot },
    };
}
