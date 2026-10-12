#pragma once

#include "Parser.h"

#include <string>

namespace BrightBox3D::HeaderTool
{
    struct GeneratorResult
    {
        std::string code;
        std::vector<Diagnostic> errors;
    };

    GeneratorResult Generate(const ParseResult& parsed, const std::string& includePath, const std::string& uniqueId);
}
