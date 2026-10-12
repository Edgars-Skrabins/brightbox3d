#pragma once

#include <string>
#include <vector>

namespace BrightBox3D::HeaderTool
{
    enum class TokenType
    {
        Identifier,
        Number,
        String,
        Character,
        Symbol
    };

    struct Token
    {
        TokenType type = TokenType::Symbol;
        std::string text;
        int line = 0;

        bool Is(const char* value) const { return text == value; }
    };

    std::vector<Token> Tokenize(const std::string& source);
}
