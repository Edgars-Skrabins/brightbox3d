#pragma once

#include "Core/Result.h"
#include "RHI/Types.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace BrightBox3D::RHI
{
    struct MetalShaderSource
    {
        std::string source;
        std::string entryPoint;
    };

    class ShaderCompiler
    {
    public:
        static Core::Result<std::string> Preprocess(const std::filesystem::path& file);
        static Core::Result<std::string> PreprocessSource(const std::string& source, const std::filesystem::path& includeDirectory);
        static std::string BuildDefinePreamble(const std::vector<std::pair<std::string, std::string>>& defines);
        static Core::Result<MetalShaderSource> CompileToMetal(const ShaderDesc& desc);
    };
}
