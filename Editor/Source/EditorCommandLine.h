#pragma once

#include "Core/CommandLine.h"

#include <filesystem>
#include <string>

namespace BrightBox3D::Editor
{
    struct EditorOptions
    {
        std::filesystem::path project;
        std::filesystem::path screenshot;
        std::filesystem::path playScreenshot;
        int frames = -1;
        std::string select;
        bool build = false;
        bool play = false;
        std::filesystem::path createProject;
        std::string addPanel;
    };

    inline const Core::CommandLineOption<EditorOptions> EditorCommandLine[] = {
        { nullptr, &EditorOptions::project },
        { "--screenshot", &EditorOptions::screenshot },
        { "--play-screenshot", &EditorOptions::playScreenshot },
        { "--frames", &EditorOptions::frames },
        { "--select", &EditorOptions::select },
        { "--build", &EditorOptions::build },
        { "--play", &EditorOptions::play },
        { "--create-project", &EditorOptions::createProject },
        { "--add-panel", &EditorOptions::addPanel },
    };
}
