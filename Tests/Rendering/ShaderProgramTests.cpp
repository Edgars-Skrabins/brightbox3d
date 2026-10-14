#include "Common/TestUtilities.h"
#include "Rendering/ShaderLibrary.h"
#include "Rendering/ShaderProgram.h"
#include "RHI/Device.h"

#include <gtest/gtest.h>

#include <unordered_set>

using namespace BrightBox3D;
using Rendering::ShaderProgram;

TEST(ShaderProgramTests, EveryProgramCompilesBothStages)
{
    std::unique_ptr<Runtime::RenderContext> context = Tests::CreateTestRenderContext();
    ASSERT_NE(context, nullptr);
    Rendering::ShaderLibrary& shaders = context->GetShaders();
    for (int index = 0; index < static_cast<int>(ShaderProgram::Count); ++index)
    {
        ShaderProgram program = static_cast<ShaderProgram>(index);
        const Rendering::ShaderProgramInfo& info = Rendering::GetShaderProgramInfo(program);
        EXPECT_NE(shaders.Get(program, RHI::ShaderStage::Vertex), nullptr) << info.name;
        EXPECT_NE(shaders.Get(program, RHI::ShaderStage::Pixel), nullptr) << info.name;
    }
}

TEST(ShaderProgramTests, PipelineKeysDifferByEveryField)
{
    Rendering::PipelineKeyHash hash;
    Rendering::PipelineKey base{ ShaderProgram::Lines, RHI::Format::RGBA8Unorm, RHI::CullMode::None, false };
    std::unordered_set<size_t> hashes = { hash(base) };
    Rendering::PipelineKey program = base;
    program.program = ShaderProgram::Tonemap;
    Rendering::PipelineKey format = base;
    format.format = RHI::Format::BGRA8Unorm;
    Rendering::PipelineKey cull = base;
    cull.cull = RHI::CullMode::Back;
    Rendering::PipelineKey depth = base;
    depth.depthTest = true;
    for (const Rendering::PipelineKey& key : { program, format, cull, depth })
    {
        EXPECT_FALSE(key == base);
        hashes.insert(hash(key));
    }
    EXPECT_EQ(hashes.size(), 5u);
}
