#pragma once

#include <string>

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Rendering
{
    struct RenderStatistics;
}

namespace BrightBox3D::Editor::StatusBar
{
    void Draw(const std::string& deviceName, const Rendering::RenderStatistics& statistics, const Level::Level* level);
}
