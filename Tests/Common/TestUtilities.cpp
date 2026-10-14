#include "Common/TestUtilities.h"

#include "Core/Guid.h"

#include <cstdlib>

namespace BrightBox3D::Tests
{
    namespace
    {
        constexpr const char* TempPrefix = "BrightTests_";
        constexpr const char* OutputVariable = "BRIGHT_TEST_OUTPUT";
    }

    std::filesystem::path MakeTempDirectory(const std::string& name)
    {
        std::filesystem::path directory = std::filesystem::temp_directory_path() / (TempPrefix + name + "_" + Core::Guid::Generate().ToString());
        std::filesystem::create_directories(directory);
        return directory;
    }

    std::filesystem::path TestOutputPath(const std::string& fileName)
    {
        const char* directory = std::getenv(OutputVariable);
        return (directory ? std::filesystem::path(directory) : std::filesystem::temp_directory_path()) / fileName;
    }

    std::unique_ptr<Runtime::RenderContext> CreateTestRenderContext()
    {
        return Runtime::RenderContext::Create(nullptr, BRIGHT_ENGINE_CONTENT_DIR "/Shaders");
    }
}
