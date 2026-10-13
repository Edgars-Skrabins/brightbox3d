#include "UI/Style.h"

#include "Core/StringUtils.h"
#include "UI/Element.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace BrightBox3D::UI
{
    namespace
    {
        std::string StripComments(const std::string& text)
        {
            std::string result;
            for (size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*')
                {
                    size_t end = text.find("*/", i + 2);
                    if (end == std::string::npos)
                    {
                        break;
                    }
                    i = end + 1;
                    continue;
                }
                result += text[i];
            }
            return result;
        }

        SimpleSelector ParseSimple(const std::string& text, int& specificity)
        {
            SimpleSelector selector;
            size_t i = 0;
            auto readName = [&]()
            {
                size_t start = i;
                while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '-' || text[i] == '_'))
                {
                    ++i;
                }
                return text.substr(start, i - start);
            };
            while (i < text.size())
            {
                char c = text[i];
                if (c == '.')
                {
                    ++i;
                    selector.classes.push_back(readName());
                    specificity += 10;
                }
                else if (c == '#')
                {
                    ++i;
                    selector.id = readName();
                    specificity += 100;
                }
                else if (c == ':')
                {
                    ++i;
                    selector.pseudo.push_back(readName());
                    specificity += 10;
                }
                else if (c == '*')
                {
                    ++i;
                }
                else
                {
                    selector.type = readName();
                    specificity += 1;
                    if (selector.type.empty())
                    {
                        ++i;
                    }
                }
            }
            return selector;
        }

        bool MatchesSimple(const SimpleSelector& selector, const Element& element)
        {
            if (!selector.type.empty() && selector.type != element.GetTypeName())
            {
                return false;
            }
            if (!selector.id.empty() && selector.id != element.GetName())
            {
                return false;
            }
            for (const std::string& name : selector.classes)
            {
                if (!element.HasClass(name))
                {
                    return false;
                }
            }
            for (const std::string& pseudo : selector.pseudo)
            {
                if ((pseudo == "hover" && !element.IsHovered()) || (pseudo == "active" && !element.IsPressed()) || (pseudo == "focus" && !element.IsFocused()) || (pseudo == "disabled" && !element.IsDisabled()) || (pseudo == "checked" && !element.GetBool()))
                {
                    return false;
                }
            }
            return true;
        }

        void ApplyBox(Length (&box)[4], const std::string& value)
        {
            std::vector<std::string> parts;
            for (const std::string& part : Core::Split(Core::Trim(value), ' '))
            {
                if (!part.empty())
                {
                    parts.push_back(part);
                }
            }
            if (parts.empty())
            {
                return;
            }
            Length top = ParseLength(parts[0]);
            Length right = parts.size() > 1 ? ParseLength(parts[1]) : top;
            Length bottom = parts.size() > 2 ? ParseLength(parts[2]) : top;
            Length left = parts.size() > 3 ? ParseLength(parts[3]) : right;
            box[0] = top;
            box[1] = right;
            box[2] = bottom;
            box[3] = left;
        }

        float ParseNumber(const std::string& text)
        {
            return static_cast<float>(std::atof(text.c_str()));
        }
    }

    Length ParseLength(const std::string& input)
    {
        std::string text = Core::ToLower(Core::Trim(input));
        if (text.empty())
        {
            return {};
        }
        if (text == "auto")
        {
            return Length::Auto();
        }
        if (Core::EndsWith(text, "%"))
        {
            return Length::Percent(ParseNumber(text.substr(0, text.size() - 1)));
        }
        if (Core::EndsWith(text, "px"))
        {
            return Length::Pixels(ParseNumber(text.substr(0, text.size() - 2)));
        }
        return Length::Pixels(ParseNumber(text));
    }

    bool ParseColor(const std::string& input, Math::Color& color)
    {
        std::string text = Core::ToLower(Core::Trim(input));
        auto hex = [](char c) -> int
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
        };
        if (!text.empty() && text[0] == '#')
        {
            std::string digits = text.substr(1);
            if (digits.size() == 3 || digits.size() == 4)
            {
                float components[4] = { 1, 1, 1, 1 };
                for (size_t i = 0; i < digits.size(); ++i)
                {
                    components[i] = hex(digits[i]) * 17 / 255.0f;
                }
                color = { components[0], components[1], components[2], components[3] };
                return true;
            }
            if (digits.size() == 6 || digits.size() == 8)
            {
                float components[4] = { 1, 1, 1, 1 };
                for (size_t i = 0; i < digits.size() / 2; ++i)
                {
                    components[i] = (hex(digits[i * 2]) * 16 + hex(digits[i * 2 + 1])) / 255.0f;
                }
                color = { components[0], components[1], components[2], components[3] };
                return true;
            }
            return false;
        }
        if (Core::StartsWith(text, "rgb"))
        {
            size_t open = text.find('(');
            size_t close = text.find(')');
            if (open == std::string::npos || close == std::string::npos)
            {
                return false;
            }
            std::vector<std::string> parts = Core::Split(text.substr(open + 1, close - open - 1), ',');
            if (parts.size() < 3)
            {
                return false;
            }
            color = { ParseNumber(parts[0]) / 255.0f, ParseNumber(parts[1]) / 255.0f, ParseNumber(parts[2]) / 255.0f, parts.size() > 3 ? ParseNumber(parts[3]) : 1.0f };
            return true;
        }
        static const std::unordered_map<std::string, Math::Color> named = {
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
            { "transparent", { 0, 0, 0, 0 } }
        };
        auto it = named.find(text);
        if (it != named.end())
        {
            color = it->second;
            return true;
        }
        return false;
    }

    std::vector<std::pair<std::string, std::string>> ParseDeclarations(const std::string& text)
    {
        std::vector<std::pair<std::string, std::string>> declarations;
        for (const std::string& part : Core::Split(text, ';'))
        {
            size_t colon = part.find(':');
            if (colon == std::string::npos)
            {
                continue;
            }
            std::string property = Core::ToLower(Core::Trim(part.substr(0, colon)));
            std::string value = Core::Trim(part.substr(colon + 1));
            if (!property.empty())
            {
                declarations.emplace_back(property, value);
            }
        }
        return declarations;
    }

    bool StyleSheet::Parse(const std::string& source, std::string& error)
    {
        std::string text = StripComments(source);
        size_t position = 0;
        uint32_t order = static_cast<uint32_t>(m_rules.size());
        while (true)
        {
            size_t open = text.find('{', position);
            if (open == std::string::npos)
            {
                break;
            }
            size_t close = text.find('}', open);
            if (close == std::string::npos)
            {
                error = "missing '}'";
                return false;
            }
            StyleRule rule;
            rule.order = order++;
            for (const std::string& selectorText : Core::Split(text.substr(position, open - position), ','))
            {
                std::string trimmed = Core::Trim(selectorText);
                if (trimmed.empty())
                {
                    continue;
                }
                Selector selector;
                for (const std::string& token : Core::Split(trimmed, ' '))
                {
                    if (!token.empty())
                    {
                        selector.parts.push_back(ParseSimple(token, selector.specificity));
                    }
                }
                if (!selector.parts.empty())
                {
                    rule.selectors.push_back(selector);
                }
            }
            rule.declarations = ParseDeclarations(text.substr(open + 1, close - open - 1));
            if (!rule.selectors.empty())
            {
                m_rules.push_back(std::move(rule));
            }
            position = close + 1;
        }
        return true;
    }

    void StyleSheet::Append(const StyleSheet& other)
    {
        uint32_t base = static_cast<uint32_t>(m_rules.size());
        for (StyleRule rule : other.m_rules)
        {
            rule.order += base;
            m_rules.push_back(std::move(rule));
        }
    }

    bool Matches(const Selector& selector, const Element& element)
    {
        if (selector.parts.empty() || !MatchesSimple(selector.parts.back(), element))
        {
            return false;
        }
        const Element* ancestor = element.GetParent();
        for (int index = static_cast<int>(selector.parts.size()) - 2; index >= 0; --index)
        {
            while (ancestor && !MatchesSimple(selector.parts[static_cast<size_t>(index)], *ancestor))
            {
                ancestor = ancestor->GetParent();
            }
            if (!ancestor)
            {
                return false;
            }
            ancestor = ancestor->GetParent();
        }
        return true;
    }

    void ApplyDeclaration(ComputedStyle& style, const std::string& property, const std::string& value)
    {
        std::string lower = Core::ToLower(value);
        if (property == "display")
        {
            style.displayNone = lower == "none";
        }
        else if (property == "flex-direction")
        {
            style.flexDirection = lower;
        }
        else if (property == "justify-content")
        {
            style.justifyContent = lower;
        }
        else if (property == "align-items")
        {
            style.alignItems = lower;
        }
        else if (property == "align-self")
        {
            style.alignSelf = lower;
        }
        else if (property == "align-content")
        {
            style.alignContent = lower;
        }
        else if (property == "flex-wrap")
        {
            style.flexWrap = lower;
        }
        else if (property == "position")
        {
            style.position = lower;
        }
        else if (property == "overflow")
        {
            style.overflow = lower;
        }
        else if (property == "flex-grow")
        {
            style.flexGrow = ParseNumber(value);
        }
        else if (property == "flex-shrink")
        {
            style.flexShrink = ParseNumber(value);
        }
        else if (property == "flex-basis")
        {
            style.flexBasis = ParseLength(value);
        }
        else if (property == "flex")
        {
            std::vector<std::string> parts = Core::Split(Core::Trim(value), ' ');
            style.flexGrow = parts.empty() ? 0.0f : ParseNumber(parts[0]);
            style.flexShrink = parts.size() > 1 ? ParseNumber(parts[1]) : 1.0f;
            style.flexBasis = parts.size() > 2 ? ParseLength(parts[2]) : Length::Pixels(0.0f);
        }
        else if (property == "width")
        {
            style.width = ParseLength(value);
        }
        else if (property == "height")
        {
            style.height = ParseLength(value);
        }
        else if (property == "min-width")
        {
            style.minWidth = ParseLength(value);
        }
        else if (property == "min-height")
        {
            style.minHeight = ParseLength(value);
        }
        else if (property == "max-width")
        {
            style.maxWidth = ParseLength(value);
        }
        else if (property == "max-height")
        {
            style.maxHeight = ParseLength(value);
        }
        else if (property == "margin")
        {
            ApplyBox(style.margin, value);
        }
        else if (property == "padding")
        {
            ApplyBox(style.padding, value);
        }
        else if (property == "margin-top" || property == "margin-right" || property == "margin-bottom" || property == "margin-left")
        {
            int index = property == "margin-top" ? 0 : (property == "margin-right" ? 1 : (property == "margin-bottom" ? 2 : 3));
            style.margin[index] = ParseLength(value);
        }
        else if (property == "padding-top" || property == "padding-right" || property == "padding-bottom" || property == "padding-left")
        {
            int index = property == "padding-top" ? 0 : (property == "padding-right" ? 1 : (property == "padding-bottom" ? 2 : 3));
            style.padding[index] = ParseLength(value);
        }
        else if (property == "top" || property == "right" || property == "bottom" || property == "left")
        {
            int index = property == "top" ? 0 : (property == "right" ? 1 : (property == "bottom" ? 2 : 3));
            style.inset[index] = ParseLength(value);
        }
        else if (property == "gap")
        {
            style.gap = ParseLength(value).value;
        }
        else if (property == "border-width")
        {
            style.borderWidth = ParseLength(value).value;
        }
        else if (property == "border-radius")
        {
            style.borderRadius = ParseLength(value).value;
        }
        else if (property == "border")
        {
            for (const std::string& part : Core::Split(Core::Trim(value), ' '))
            {
                Math::Color color;
                if (ParseColor(part, color))
                {
                    style.borderColor = color;
                }
                else if (!part.empty() && (std::isdigit(static_cast<unsigned char>(part[0])) || part[0] == '.'))
                {
                    style.borderWidth = ParseLength(part).value;
                }
            }
        }
        else if (property == "background-color" || property == "background")
        {
            ParseColor(value, style.backgroundColor);
        }
        else if (property == "border-color")
        {
            ParseColor(value, style.borderColor);
        }
        else if (property == "color")
        {
            ParseColor(value, style.color);
        }
        else if (property == "fill-color")
        {
            ParseColor(value, style.fillColor);
        }
        else if (property == "font-size")
        {
            style.fontSize = ParseLength(value).value;
        }
        else if (property == "font-family")
        {
            std::string family = Core::Trim(value);
            if (family.size() >= 2 && (family.front() == '"' || family.front() == '\''))
            {
                family = family.substr(1, family.size() - 2);
            }
            style.fontFamily = family;
        }
        else if (property == "text-align")
        {
            style.textAlign = lower == "center" ? TextAlign::Center : (lower == "right" ? TextAlign::Right : TextAlign::Left);
        }
        else if (property == "opacity")
        {
            style.opacity = ParseNumber(value);
        }
    }
}
