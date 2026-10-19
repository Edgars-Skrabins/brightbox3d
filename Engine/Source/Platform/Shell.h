#pragma once

#include "Core/Result.h"

#include <filesystem>

namespace BrightBox3D::Platform::Shell
{
    void OpenInCodeEditor(const std::filesystem::path& file, int line);
    void RevealInFileBrowser(const std::filesystem::path& path);
    Core::Result<void> MoveToTrash(const std::filesystem::path& path);
    std::filesystem::path UserDirectory();
    void Shutdown();
}
