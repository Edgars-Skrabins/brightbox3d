#pragma once

#include "Runtime/RenderContext.h"

#include <filesystem>
#include <memory>
#include <string>

namespace BrightBox3D::Tests
{
    std::filesystem::path MakeTempDirectory(const std::string& name);
    std::filesystem::path TestOutputPath(const std::string& fileName);
    std::unique_ptr<Runtime::RenderContext> CreateTestRenderContext();
}
