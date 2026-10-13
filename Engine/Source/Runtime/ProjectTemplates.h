#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace BrightBox3D::Runtime::ProjectTemplates
{
    inline constexpr const char* GeneratedMarker = "bright_add_game_module";

    std::optional<std::string> GameCMakeLists(const std::string& moduleName, const std::filesystem::path& sdk);
    std::optional<std::string> GitIgnore();
}
