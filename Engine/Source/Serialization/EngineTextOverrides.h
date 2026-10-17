#pragma once

#include "Core/Result.h"

#include <filesystem>
#include <string>
#include <vector>

namespace BrightBox3D::Serialization
{
    Core::Result<std::vector<std::string>> LoadEngineTextOverrides(const std::filesystem::path& path);
}
