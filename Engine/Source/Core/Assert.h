#pragma once

#include <string>

namespace BrightBox3D::Core
{
    [[noreturn]] void AssertionFailed(const char* expression, const std::string& message, const char* file, int line);
}

#define BrightAssert(condition, message) \
    do \
    { \
        if (!(condition)) \
        { \
            ::BrightBox3D::Core::AssertionFailed(#condition, message, __FILE__, __LINE__); \
        } \
    } while (false)
