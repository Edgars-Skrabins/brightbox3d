#pragma once

#include "Math/Color.h"
#include "UI/CssKeywords.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace BrightBox3D::UI
{
    class Element;

    enum class LengthUnit
    {
        Undefined,
        Auto,
        Pixels,
        Percent
    };

    struct Length
    {
        LengthUnit unit = LengthUnit::Undefined;
        float value = 0.0f;

        static Length Pixels(float value) { return { LengthUnit::Pixels, value }; }
        static Length Percent(float value) { return { LengthUnit::Percent, value }; }
        static Length Auto() { return { LengthUnit::Auto, 0.0f }; }
        bool IsSet() const { return unit != LengthUnit::Undefined; }
    };

    enum BoxSide
    {
        BoxTop,
        BoxRight,
        BoxBottom,
        BoxLeft,
        BoxSideCount
    };

    struct ComputedStyle
    {
        Display display = Display::Flex;
        FlexDirection flexDirection = FlexDirection::Column;
        Justify justifyContent = Justify::FlexStart;
        Align alignItems = Align::Stretch;
        Align alignSelf = Align::Auto;
        Align alignContent = Align::FlexStart;
        FlexWrap flexWrap = FlexWrap::NoWrap;
        PositionType position = PositionType::Relative;
        Overflow overflow = Overflow::Visible;
        float flexGrow = 0.0f;
        float flexShrink = 1.0f;
        Length flexBasis = Length::Auto();
        Length width;
        Length height;
        Length minWidth;
        Length minHeight;
        Length maxWidth;
        Length maxHeight;
        Length margin[BoxSideCount];
        Length padding[BoxSideCount];
        Length inset[BoxSideCount];
        float gap = 0.0f;
        float borderWidth = 0.0f;
        float borderRadius = 0.0f;
        Math::Color backgroundColor = Math::Color::Clear();
        Math::Color borderColor = Math::Color::Clear();
        Math::Color color = Math::Color::White();
        Math::Color fillColor{ 0.25f, 0.55f, 0.95f, 1.0f };
        float fontSize = 16.0f;
        std::string fontFamily;
        TextAlign textAlign = TextAlign::Left;
        float opacity = 1.0f;

        bool IsHidden() const { return display == Display::None; }
    };

    using CssSetter = std::function<void(ComputedStyle&)>;

    struct CssDeclaration
    {
        CssSetter apply;
    };

    struct SimpleSelector
    {
        std::string type;
        std::string id;
        std::vector<std::string> classes;
        std::vector<PseudoClass> pseudo;
    };

    struct Selector
    {
        std::vector<SimpleSelector> parts;
        int specificity = 0;
    };

    struct StyleRule
    {
        std::vector<Selector> selectors;
        std::vector<CssDeclaration> declarations;
        uint32_t order = 0;
    };
}
