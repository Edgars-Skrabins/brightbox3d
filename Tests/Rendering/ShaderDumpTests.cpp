#include "RHI/ShaderCompiler.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>

using namespace BrightBox3D::RHI;

TEST(ShaderDumpTests, DumpMetal)
{
    const char* file = std::getenv("BRIGHT_DUMP_SHADER");
    const char* entry = std::getenv("BRIGHT_DUMP_ENTRY");
    if (!file || !entry)
    {
        GTEST_SKIP();
    }
    std::string source;
    std::string error;
    ASSERT_TRUE(ShaderCompiler::Preprocess(std::string(BRIGHT_ENGINE_CONTENT_DIR "/Shaders/") + file, source, error)) << error;
    ShaderDesc desc;
    desc.source = source;
    desc.entryPoint = entry;
    desc.stage = std::getenv("BRIGHT_DUMP_VERTEX") ? ShaderStage::Vertex : ShaderStage::Pixel;
    ShaderCompileResult result = ShaderCompiler::CompileToMetal(desc);
    ASSERT_TRUE(result.success) << result.error;
    std::cout << result.metalSource << std::endl;
}
