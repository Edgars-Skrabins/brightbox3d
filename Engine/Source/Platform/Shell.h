#pragma once

#include <filesystem>

namespace BrightBox3D::Platform::Shell
{
    void OpenInCodeEditor(const std::filesystem::path& file, int line);
    void RevealInFileBrowser(const std::filesystem::path& path);
    std::filesystem::path UserDirectory();
    void Shutdown();
}
