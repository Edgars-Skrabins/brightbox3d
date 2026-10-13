#include "UI/CssProperties.h"

#include "Core/EngineText.h"
#include "Core/StringUtils.h"
#include "UI/CssValues.h"

#include <cctype>
#include <functional>
#include <iterator>
#include <span>

namespace BrightBox3D::UI
{
    namespace
    {
        using CssValueParser = std::function<std::optional<CssSetter>(std::string_view)>;
        using BoxField = Length (ComputedStyle::*)[BoxSideCount];

        struct CssPropertyInfo
        {
            const char* name;
            CssValueParser parse;
        };

        template <typename Value>
        CssValueParser KeywordProperty(Value ComputedStyle::* field, std::span<const CssKeyword<Value>> keywords)
        {
            return [field, keywords](std::string_view text) -> std::optional<CssSetter>
            {
                std::optional<Value> value = ParseKeyword(keywords, Core::ToLower(Core::Trim(text)));
                if (!value)
                {
                    return std::nullopt;
                }
                return [field, parsed = *value](ComputedStyle& style) { style.*field = parsed; };
            };
        }

        CssValueParser NumberProperty(float ComputedStyle::* field)
        {
            return [field](std::string_view text) -> std::optional<CssSetter>
            {
                return [field, value = ParseNumber(text)](ComputedStyle& style) { style.*field = value; };
            };
        }

        CssValueParser LengthProperty(Length ComputedStyle::* field)
        {
            return [field](std::string_view text) -> std::optional<CssSetter>
            {
                return [field, value = ParseLength(text)](ComputedStyle& style) { style.*field = value; };
            };
        }

        CssValueParser PixelProperty(float ComputedStyle::* field)
        {
            return [field](std::string_view text) -> std::optional<CssSetter>
            {
                return [field, value = ParseLength(text).value](ComputedStyle& style) { style.*field = value; };
            };
        }

        CssValueParser ColorProperty(Math::Color ComputedStyle::* field)
        {
            return [field](std::string_view text) -> std::optional<CssSetter>
            {
                Math::Color color;
                if (!ParseColor(text, color))
                {
                    return std::nullopt;
                }
                return [field, color](ComputedStyle& style) { style.*field = color; };
            };
        }

        CssValueParser BoxProperty(BoxField field)
        {
            return [field](std::string_view text) -> std::optional<CssSetter>
            {
                std::optional<std::array<Length, BoxSideCount>> box = ParseBox(text);
                if (!box)
                {
                    return std::nullopt;
                }
                return [field, sides = *box](ComputedStyle& style)
                {
                    for (int side = 0; side < BoxSideCount; ++side)
                    {
                        (style.*field)[side] = sides[side];
                    }
                };
            };
        }

        CssValueParser BoxSideProperty(BoxField field, BoxSide side)
        {
            return [field, side](std::string_view text) -> std::optional<CssSetter>
            {
                return [field, side, value = ParseLength(text)](ComputedStyle& style) { (style.*field)[side] = value; };
            };
        }

        std::optional<CssSetter> ParseFlex(std::string_view text)
        {
            std::vector<std::string> parts = Core::Split(Core::Trim(text), ' ');
            float grow = parts.empty() ? 0.0f : ParseNumber(parts[0]);
            float shrink = parts.size() > 1 ? ParseNumber(parts[1]) : 1.0f;
            Length basis = parts.size() > 2 ? ParseLength(parts[2]) : Length::Pixels(0.0f);
            return [grow, shrink, basis](ComputedStyle& style)
            {
                style.flexGrow = grow;
                style.flexShrink = shrink;
                style.flexBasis = basis;
            };
        }

        std::optional<CssSetter> ParseBorder(std::string_view text)
        {
            std::optional<Math::Color> color;
            std::optional<float> width;
            for (const std::string& part : Core::Split(Core::Trim(text), ' '))
            {
                Math::Color parsed;
                if (ParseColor(part, parsed))
                {
                    color = parsed;
                }
                else if (!part.empty() && (std::isdigit(static_cast<unsigned char>(part[0])) || part[0] == '.'))
                {
                    width = ParseLength(part).value;
                }
            }
            return [color, width](ComputedStyle& style)
            {
                if (color)
                {
                    style.borderColor = *color;
                }
                if (width)
                {
                    style.borderWidth = *width;
                }
            };
        }

        std::optional<CssSetter> ParseFontFamilyValue(std::string_view text)
        {
            return [family = ParseFontFamily(text)](ComputedStyle& style) { style.fontFamily = family; };
        }

        const CssPropertyInfo Properties[] = {
            { "display", KeywordProperty<Display>(&ComputedStyle::display, DisplayKeywords) },
            { "flex-direction", KeywordProperty<FlexDirection>(&ComputedStyle::flexDirection, FlexDirectionKeywords) },
            { "justify-content", KeywordProperty<Justify>(&ComputedStyle::justifyContent, JustifyKeywords) },
            { "align-items", KeywordProperty<Align>(&ComputedStyle::alignItems, AlignKeywords) },
            { "align-self", KeywordProperty<Align>(&ComputedStyle::alignSelf, AlignKeywords) },
            { "align-content", KeywordProperty<Align>(&ComputedStyle::alignContent, AlignKeywords) },
            { "flex-wrap", KeywordProperty<FlexWrap>(&ComputedStyle::flexWrap, FlexWrapKeywords) },
            { "position", KeywordProperty<PositionType>(&ComputedStyle::position, PositionKeywords) },
            { "overflow", KeywordProperty<Overflow>(&ComputedStyle::overflow, OverflowKeywords) },
            { "flex-grow", NumberProperty(&ComputedStyle::flexGrow) },
            { "flex-shrink", NumberProperty(&ComputedStyle::flexShrink) },
            { "flex-basis", LengthProperty(&ComputedStyle::flexBasis) },
            { "flex", ParseFlex },
            { "width", LengthProperty(&ComputedStyle::width) },
            { "height", LengthProperty(&ComputedStyle::height) },
            { "min-width", LengthProperty(&ComputedStyle::minWidth) },
            { "min-height", LengthProperty(&ComputedStyle::minHeight) },
            { "max-width", LengthProperty(&ComputedStyle::maxWidth) },
            { "max-height", LengthProperty(&ComputedStyle::maxHeight) },
            { "margin", BoxProperty(&ComputedStyle::margin) },
            { "padding", BoxProperty(&ComputedStyle::padding) },
            { "margin-top", BoxSideProperty(&ComputedStyle::margin, BoxTop) },
            { "margin-right", BoxSideProperty(&ComputedStyle::margin, BoxRight) },
            { "margin-bottom", BoxSideProperty(&ComputedStyle::margin, BoxBottom) },
            { "margin-left", BoxSideProperty(&ComputedStyle::margin, BoxLeft) },
            { "padding-top", BoxSideProperty(&ComputedStyle::padding, BoxTop) },
            { "padding-right", BoxSideProperty(&ComputedStyle::padding, BoxRight) },
            { "padding-bottom", BoxSideProperty(&ComputedStyle::padding, BoxBottom) },
            { "padding-left", BoxSideProperty(&ComputedStyle::padding, BoxLeft) },
            { "top", BoxSideProperty(&ComputedStyle::inset, BoxTop) },
            { "right", BoxSideProperty(&ComputedStyle::inset, BoxRight) },
            { "bottom", BoxSideProperty(&ComputedStyle::inset, BoxBottom) },
            { "left", BoxSideProperty(&ComputedStyle::inset, BoxLeft) },
            { "gap", PixelProperty(&ComputedStyle::gap) },
            { "border-width", PixelProperty(&ComputedStyle::borderWidth) },
            { "border-radius", PixelProperty(&ComputedStyle::borderRadius) },
            { "border", ParseBorder },
            { "background-color", ColorProperty(&ComputedStyle::backgroundColor) },
            { "background", ColorProperty(&ComputedStyle::backgroundColor) },
            { "border-color", ColorProperty(&ComputedStyle::borderColor) },
            { "color", ColorProperty(&ComputedStyle::color) },
            { "fill-color", ColorProperty(&ComputedStyle::fillColor) },
            { "font-size", PixelProperty(&ComputedStyle::fontSize) },
            { "font-family", ParseFontFamilyValue },
            { "text-align", KeywordProperty<TextAlign>(&ComputedStyle::textAlign, TextAlignKeywords) },
            { "opacity", NumberProperty(&ComputedStyle::opacity) },
        };

        static_assert(std::size(Properties) == static_cast<size_t>(CssProperty::Count));

        std::string UnknownPropertyWarning(std::string_view name)
        {
            return Core::Text(Core::EngineText::CssUnknownProperty, name);
        }

        std::string InvalidValueWarning(std::string_view name, std::string_view value)
        {
            return Core::Text(Core::EngineText::CssInvalidValue, value, name);
        }
    }

    std::optional<CssProperty> FindCssProperty(std::string_view name)
    {
        for (size_t index = 0; index < std::size(Properties); ++index)
        {
            if (name == Properties[index].name)
            {
                return static_cast<CssProperty>(index);
            }
        }
        return std::nullopt;
    }

    const char* CssPropertyName(CssProperty property)
    {
        return Properties[static_cast<size_t>(property)].name;
    }

    std::optional<CssDeclaration> ParseDeclaration(CssProperty property, std::string_view value)
    {
        std::optional<CssSetter> setter = Properties[static_cast<size_t>(property)].parse(value);
        if (!setter)
        {
            return std::nullopt;
        }
        return CssDeclaration{ std::move(*setter) };
    }

    std::vector<CssDeclaration> ParseDeclarations(std::string_view text, std::vector<std::string>& warnings)
    {
        std::vector<CssDeclaration> declarations;
        for (const std::string& part : Core::Split(text, ';'))
        {
            size_t colon = part.find(':');
            if (colon == std::string::npos)
            {
                continue;
            }
            std::string name = Core::ToLower(Core::Trim(std::string_view(part).substr(0, colon)));
            std::string value = Core::Trim(std::string_view(part).substr(colon + 1));
            if (name.empty())
            {
                continue;
            }
            std::optional<CssProperty> property = FindCssProperty(name);
            if (!property)
            {
                warnings.push_back(UnknownPropertyWarning(name));
                continue;
            }
            std::optional<CssDeclaration> declaration = ParseDeclaration(*property, value);
            if (!declaration)
            {
                warnings.push_back(InvalidValueWarning(name, value));
                continue;
            }
            declarations.push_back(std::move(*declaration));
        }
        return declarations;
    }
}
