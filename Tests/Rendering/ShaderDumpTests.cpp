#include "RHI/ShaderCompiler.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>

using namespace BrightBox3D;
using namespace BrightBox3D::RHI;

TEST(ShaderDumpTests, DumpMetal)
{
    const char* file = std::getenv("BRIGHT_DUMP_SHADER");
    const char* entry = std::getenv("BRIGHT_DUMP_ENTRY");
    if (!file || !entry)
    {
        GTEST_SKIP();
    }
    Core::Result<std::string> source = ShaderCompiler::Preprocess(std::string(BRIGHT_ENGINE_CONTENT_DIR "/Shaders/") + file);
    ASSERT_TRUE(source) << source.GetError().GetText();
    ShaderDesc desc;
    desc.source = *source;
    desc.entryPoint = entry;
    desc.stage = std::getenv("BRIGHT_DUMP_VERTEX") ? ShaderStage::Vertex : ShaderStage::Pixel;
    Core::Result<MetalShaderSource> result = ShaderCompiler::CompileToMetal(desc);
    ASSERT_TRUE(result) << result.GetError().GetText();
    std::cout << result->source << std::endl;
}
