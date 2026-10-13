#pragma once

#include "UI/Style.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace BrightBox3D::UI
{
    enum class CssProperty
    {
        Display,
        FlexDirection,
        JustifyContent,
        AlignItems,
        AlignSelf,
        AlignContent,
        FlexWrap,
        Position,
        Overflow,
        FlexGrow,
        FlexShrink,
        FlexBasis,
        Flex,
        Width,
        Height,
        MinWidth,
        MinHeight,
        MaxWidth,
        MaxHeight,
        Margin,
        Padding,
        MarginTop,
        MarginRight,
        MarginBottom,
        MarginLeft,
        PaddingTop,
        PaddingRight,
        PaddingBottom,
        PaddingLeft,
        Top,
        Right,
        Bottom,
        Left,
        Gap,
        BorderWidth,
        BorderRadius,
        Border,
        BackgroundColor,
        Background,
        BorderColor,
        Color,
        FillColor,
        FontSize,
        FontFamily,
        TextAlign,
        Opacity,
        Count
    };

    std::optional<CssProperty> FindCssProperty(std::string_view name);
    const char* CssPropertyName(CssProperty property);
    std::optional<CssDeclaration> ParseDeclaration(CssProperty property, std::string_view value);
    std::vector<CssDeclaration> ParseDeclarations(std::string_view text, std::vector<std::string>& warnings);
}
