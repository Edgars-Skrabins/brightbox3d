#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace BrightBox3D::Serialization
{
    bool LoadEngineTextOverrides(const std::filesystem::path& path, std::vector<std::string>& rejectedKeys, std::string& error);
}
