#include "Core/StringUtils.h"

#include <cctype>

namespace BrightBox3D::Core
{
    std::vector<std::string> Split(std::string_view text, char separator)
    {
        std::vector<std::string> parts;
        size_t start = 0;
        while (start <= text.size())
        {
            size_t end = text.find(separator, start);
            if (end == std::string_view::npos)
            {
                end = text.size();
            }
            parts.emplace_back(text.substr(start, end - start));
            start = end + 1;
        }
        return parts;
    }

    std::string Trim(std::string_view text)
    {
        size_t start = 0;
        while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])))
        {
            ++start;
        }
        size_t end = text.size();
        while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
        {
            --end;
        }
        return std::string(text.substr(start, end - start));
    }

    std::string ToLower(std::string_view text)
    {
        std::string result(text);
        for (char& c : result)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return result;
    }

    bool StartsWith(std::string_view text, std::string_view prefix)
    {
        return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
    }

    bool EndsWith(std::string_view text, std::string_view suffix)
    {
        return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
    }

    std::string ReplaceAll(std::string text, std::string_view from, std::string_view to)
    {
        if (from.empty())
        {
            return text;
        }
        size_t position = 0;
        while ((position = text.find(from, position)) != std::string::npos)
        {
            text.replace(position, from.size(), to);
            position += to.size();
        }
        return text;
    }

    std::string PrettifyName(std::string_view name)
    {
        if (StartsWith(name, "m_"))
        {
            name.remove_prefix(2);
        }
        std::string result;
        for (size_t i = 0; i < name.size(); ++i)
        {
            char c = name[i];
            if (c == '_')
            {
                if (!result.empty() && result.back() != ' ')
                {
                    result += ' ';
                }
                continue;
            }
            bool upper = std::isupper(static_cast<unsigned char>(c)) != 0;
            bool digit = std::isdigit(static_cast<unsigned char>(c)) != 0;
            if (i > 0 && !result.empty() && result.back() != ' ')
            {
                char previous = name[i - 1];
                bool previousLower = std::islower(static_cast<unsigned char>(previous)) != 0;
                bool previousDigit = std::isdigit(static_cast<unsigned char>(previous)) != 0;
                bool nextLower = i + 1 < name.size() && std::islower(static_cast<unsigned char>(name[i + 1])) != 0;
                bool previousUpper = std::isupper(static_cast<unsigned char>(previous)) != 0;
                if ((upper && (previousLower || previousDigit)) || (upper && previousUpper && nextLower) || (digit && !previousDigit))
                {
                    result += ' ';
                }
            }
            if (result.empty() || result.back() == ' ')
            {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            result += c;
        }
        return result;
    }

    int CountFormatPlaceholders(std::string_view text)
    {
        int count = 0;
        for (size_t index = 0; index < text.size(); ++index)
        {
            bool escaped = text[index] == '{' && index + 1 < text.size() && text[index + 1] == '{';
            if (escaped)
            {
                ++index;
                continue;
            }
            count += text[index] == '{' ? 1 : 0;
        }
        return count;
    }
}
