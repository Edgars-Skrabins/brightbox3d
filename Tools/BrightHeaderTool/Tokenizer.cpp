#include "Tokenizer.h"

#include <cctype>

namespace BrightBox3D::HeaderTool
{
    namespace
    {
        bool IsIdentifierStart(char c)
        {
            return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
        }

        bool IsIdentifierPart(char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }
    }

    std::vector<Token> Tokenize(const std::string& source)
    {
        std::vector<Token> tokens;
        size_t i = 0;
        int line = 1;
        bool lineStart = true;
        const size_t length = source.size();

        while (i < length)
        {
            char c = source[i];
            if (c == '\n')
            {
                ++line;
                ++i;
                lineStart = true;
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c)))
            {
                ++i;
                continue;
            }
            if (c == '/' && i + 1 < length && source[i + 1] == '/')
            {
                while (i < length && source[i] != '\n')
                {
                    ++i;
                }
                continue;
            }
            if (c == '/' && i + 1 < length && source[i + 1] == '*')
            {
                i += 2;
                while (i + 1 < length && !(source[i] == '*' && source[i + 1] == '/'))
                {
                    if (source[i] == '\n')
                    {
                        ++line;
                    }
                    ++i;
                }
                i += 2;
                continue;
            }
            if (c == '#' && lineStart)
            {
                while (i < length && source[i] != '\n')
                {
                    if (source[i] == '\\' && i + 1 < length && source[i + 1] == '\n')
                    {
                        ++line;
                        i += 2;
                        continue;
                    }
                    ++i;
                }
                continue;
            }
            lineStart = false;

            if (c == 'R' && i + 1 < length && source[i + 1] == '"')
            {
                size_t open = source.find('(', i + 2);
                std::string delimiter = source.substr(i + 2, open - (i + 2));
                std::string terminator = ")" + delimiter + "\"";
                size_t close = source.find(terminator, open);
                size_t end = close == std::string::npos ? length : close + terminator.size();
                Token token{ TokenType::String, source.substr(i, end - i), line };
                for (size_t k = i; k < end; ++k)
                {
                    if (source[k] == '\n')
                    {
                        ++line;
                    }
                }
                tokens.push_back(token);
                i = end;
                continue;
            }
            if (IsIdentifierStart(c))
            {
                size_t start = i;
                while (i < length && IsIdentifierPart(source[i]))
                {
                    ++i;
                }
                tokens.push_back({ TokenType::Identifier, source.substr(start, i - start), line });
                continue;
            }
            if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && i + 1 < length && std::isdigit(static_cast<unsigned char>(source[i + 1]))))
            {
                size_t start = i;
                while (i < length && (IsIdentifierPart(source[i]) || source[i] == '.' || source[i] == '\'' || ((source[i] == '+' || source[i] == '-') && (source[i - 1] == 'e' || source[i - 1] == 'E'))))
                {
                    ++i;
                }
                tokens.push_back({ TokenType::Number, source.substr(start, i - start), line });
                continue;
            }
            if (c == '"' || c == '\'')
            {
                char quote = c;
                size_t start = i++;
                while (i < length && source[i] != quote)
                {
                    if (source[i] == '\\')
                    {
                        ++i;
                    }
                    else if (source[i] == '\n')
                    {
                        ++line;
                    }
                    ++i;
                }
                ++i;
                tokens.push_back({ quote == '"' ? TokenType::String : TokenType::Character, source.substr(start, i - start), line });
                continue;
            }
            if (c == ':' && i + 1 < length && source[i + 1] == ':')
            {
                tokens.push_back({ TokenType::Symbol, "::", line });
                i += 2;
                continue;
            }
            tokens.push_back({ TokenType::Symbol, std::string(1, c), line });
            ++i;
        }
        return tokens;
    }
}
