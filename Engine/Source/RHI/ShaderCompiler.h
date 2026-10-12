#pragma once

#include "RHI/Types.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace BrightBox3D::RHI
{
    struct ShaderCompileResult
    {
        bool success = false;
        std::string error;
        std::string metalSource;
        std::string metalEntryPoint;
    };

    class ShaderCompiler
    {
    public:
        static bool Preprocess(const std::filesystem::path& file, std::string& output, std::string& error);
        static bool PreprocessSource(const std::string& source, const std::filesystem::path& includeDirectory, std::string& output, std::string& error);
        static std::string BuildDefinePreamble(const std::vector<std::pair<std::string, std::string>>& defines);
        static ShaderCompileResult CompileToMetal(const ShaderDesc& desc);
    };
}
