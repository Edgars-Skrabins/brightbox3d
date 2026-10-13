#pragma once

#include <string>

namespace BrightBox3D::Reflection
{
    class TypeRedirects
    {
    public:
        static void Add(const std::string& oldName, const std::string& newName);
        static const std::string* Resolve(const std::string& oldName);
    };

    class TypeRedirectRegistration
    {
    public:
        TypeRedirectRegistration(const char* oldName, const char* newName);
    };
}
