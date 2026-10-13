#include "Core/Assert.h"

#include "Core/EngineText.h"
#include "Core/Log.h"

#include <cstdlib>

namespace BrightBox3D::Core
{
    void AssertionFailed(const char* expression, const std::string& message, const char* file, int line)
    {
        Log::Write(LogLevel::Error, Text(EngineText::AssertionFailed, expression, message), "Assert", file, line);
        std::abort();
    }
}
