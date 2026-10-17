#pragma once

#include "Core/Result.h"
#include "Runtime/RenderContext.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

namespace BrightBox3D::Tests
{
    std::filesystem::path MakeTempDirectory(const std::string& name);
    std::filesystem::path TestOutputPath(const std::string& fileName);
    std::unique_ptr<Runtime::RenderContext> CreateTestRenderContext();

    template <typename T, typename E>
    ::testing::AssertionResult Succeeded(const Core::Result<T, E>& result)
    {
        if (result)
        {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << result.GetError().GetText();
    }
}
