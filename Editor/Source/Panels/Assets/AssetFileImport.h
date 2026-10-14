#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace BrightBox3D::Editor::AssetFileImport
{
    void ImportFiles(const std::vector<std::filesystem::path>& files, const std::string& folder);
}
