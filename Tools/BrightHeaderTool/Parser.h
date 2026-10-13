#pragma once

#include "Tokenizer.h"

#include <string>
#include <utility>
#include <vector>

namespace BrightBox3D::HeaderTool
{
    struct ReflectedProperty
    {
        std::string name;
        std::string propertyMetadata;
        std::string uiMetadata;
        bool hasProperty = false;
        bool hasUIProperty = false;
        int line = 0;
    };

    struct ReflectedClass
    {
        std::string name;
        std::string qualifiedName;
        std::string baseName;
        std::string metadata;
        std::vector<ReflectedProperty> properties;
        bool hasBody = false;
        int line = 0;
    };

    struct ReflectedEnum
    {
        std::string name;
        std::string qualifiedName;
        std::vector<std::string> entries;
        int line = 0;
    };

    struct Diagnostic
    {
        int line = 0;
        std::string message;
    };

    struct ParseResult
    {
        std::vector<ReflectedClass> classes;
        std::vector<ReflectedEnum> enums;
        std::vector<Diagnostic> errors;
    };

    ParseResult Parse(const std::vector<Token>& tokens);
}
