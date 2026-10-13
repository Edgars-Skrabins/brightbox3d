#include "UI/CssValues.h"

#include "Core/StringUtils.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace BrightBox3D::UI
{
    namespace
    {
        constexpr std::string_view AutoKeyword = "auto";
        constexpr std::string_view PercentSuffix = "%";
        constexpr std::string_view PixelSuffix = "px";
        constexpr std::string_view RgbPrefix = "rgb";
        constexpr char HexPrefix = '#';
        constexpr float ChannelMax = 255.0f;
        constexpr int ShortHexScale = 17;

        struct NamedColor
        {
            std::string_view name;
            Math::Color color;
        };

        constexpr NamedColor NamedColors[] = {
            { "white", { 1, 1, 1, 1 } },
            { "black", { 0, 0, 0, 1 } },
            { "red", { 1, 0, 0, 1 } },
            { "green", { 0, 0.5f, 0, 1 } },
            { "lime", { 0, 1, 0, 1 } },
            { "blue", { 0, 0, 1, 1 } },
            { "yellow", { 1, 1, 0, 1 } },
            { "orange", { 1, 0.65f, 0, 1 } },
            { "gray", { 0.5f, 0.5f, 0.5f, 1 } },
            { "grey", { 0.5f, 0.5f, 0.5f, 1 } },
            { "transparent", { 0, 0, 0, 0 } },
        };

        int HexDigit(char c)
        {
            if (c >= '0' && c <= '9')
            {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f')
            {
                return c - 'a' + 10;
            }
            return 0;
        }

        bool ParseHexColor(std::string_view digits, Math::Color& color)
        {
            float components[4] = { 1, 1, 1, 1 };
            bool shortForm = digits.size() == 3 || digits.size() == 4;
            bool longForm = digits.size() == 6 || digits.size() == 8;
            if (!shortForm && !longForm)
            {
                return false;
            }
            size_t count = shortForm ? digits.size() : digits.size() / 2;
            for (size_t i = 0; i < count; ++i)
            {
                int value = shortForm ? HexDigit(digits[i]) * ShortHexScale : HexDigit(digits[i * 2]) * 16 + HexDigit(digits[i * 2 + 1]);
                components[i] = value / ChannelMax;
            }
            color = { components[0], components[1], components[2], components[3] };
            return true;
        }

        bool ParseRgbColor(const std::string& text, Math::Color& color)
        {
            size_t open = text.find('(');
            size_t close = text.find(')');
            if (open == std::string::npos || close == std::string::npos)
            {
                return false;
            }
            std::vector<std::string> parts = Core::Split(std::string_view(text).substr(open + 1, close - open - 1), ',');
            if (parts.size() < 3)
            {
                return false;
            }
            color = { ParseNumber(parts[0]) / ChannelMax, ParseNumber(parts[1]) / ChannelMax, ParseNumber(parts[2]) / ChannelMax, parts.size() > 3 ? ParseNumber(parts[3]) : 1.0f };
            return true;
        }
    }

    float ParseNumber(std::string_view text)
    {
        return std::strtof(std::string(text).c_str(), nullptr);
    }

    Length ParseLength(std::string_view input)
    {
        std::string text = Core::ToLower(Core::Trim(input));
        if (text.empty())
        {
            return {};
        }
        if (text == AutoKeyword)
        {
            return Length::Auto();
        }
        if (Core::EndsWith(text, PercentSuffix))
        {
            return Length::Percent(ParseNumber(std::string_view(text).substr(0, text.size() - PercentSuffix.size())));
        }
        if (Core::EndsWith(text, PixelSuffix))
        {
            return Length::Pixels(ParseNumber(std::string_view(text).substr(0, text.size() - PixelSuffix.size())));
        }
        return Length::Pixels(ParseNumber(text));
    }

    bool ParseColor(std::string_view input, Math::Color& color)
    {
        std::string text = Core::ToLower(Core::Trim(input));
        if (!text.empty() && text[0] == HexPrefix)
        {
            return ParseHexColor(std::string_view(text).substr(1), color);
        }
        if (Core::StartsWith(text, RgbPrefix))
        {
            return ParseRgbColor(text, color);
        }
        for (const NamedColor& named : NamedColors)
        {
            if (named.name == text)
            {
                color = named.color;
                return true;
            }
        }
        return false;
    }

    std::optional<std::array<Length, BoxSideCount>> ParseBox(std::string_view text)
    {
        std::vector<std::string> parts;
        for (const std::string& part : Core::Split(Core::Trim(text), ' '))
        {
            if (!part.empty())
            {
                parts.push_back(part);
            }
        }
        if (parts.empty())
        {
            return std::nullopt;
        }
        Length top = ParseLength(parts[0]);
        Length right = parts.size() > 1 ? ParseLength(parts[1]) : top;
        Length bottom = parts.size() > 2 ? ParseLength(parts[2]) : top;
        Length left = parts.size() > 3 ? ParseLength(parts[3]) : right;
        return std::array<Length, BoxSideCount>{ top, right, bottom, left };
    }

    std::string ParseFontFamily(std::string_view text)
    {
        std::string family = Core::Trim(text);
        if (family.size() >= 2 && (family.front() == '"' || family.front() == '\''))
        {
            family = family.substr(1, family.size() - 2);
        }
        return family;
    }
}
