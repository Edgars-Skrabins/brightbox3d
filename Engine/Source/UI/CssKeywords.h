#pragma once

#include <optional>
#include <span>
#include <string_view>

namespace BrightBox3D::UI
{
    enum class Display
    {
        Flex,
        None
    };

    enum class FlexDirection
    {
        Column,
        Row,
        ColumnReverse,
        RowReverse,
        Count
    };

    enum class Justify
    {
        FlexStart,
        Center,
        FlexEnd,
        SpaceBetween,
        SpaceAround,
        SpaceEvenly,
        Count
    };

    enum class Align
    {
        Auto,
        FlexStart,
        Center,
        FlexEnd,
        Stretch,
        Baseline,
        SpaceBetween,
        SpaceAround,
        Count
    };

    enum class FlexWrap
    {
        NoWrap,
        Wrap,
        Count
    };

    enum class PositionType
    {
        Relative,
        Absolute,
        Count
    };

    enum class Overflow
    {
        Visible,
        Hidden,
        Count
    };

    enum class TextAlign
    {
        Left,
        Center,
        Right
    };

    enum class PseudoClass
    {
        Hover,
        Active,
        Focus,
        Disabled,
        Checked
    };

    template <typename Value>
    struct CssKeyword
    {
        std::string_view name;
        Value value;
    };

    inline constexpr CssKeyword<Display> DisplayKeywords[] = {
        { "flex", Display::Flex },
        { "block", Display::Flex },
        { "none", Display::None },
    };

    inline constexpr CssKeyword<FlexDirection> FlexDirectionKeywords[] = {
        { "column", FlexDirection::Column },
        { "row", FlexDirection::Row },
        { "column-reverse", FlexDirection::ColumnReverse },
        { "row-reverse", FlexDirection::RowReverse },
    };

    inline constexpr CssKeyword<Justify> JustifyKeywords[] = {
        { "flex-start", Justify::FlexStart },
        { "start", Justify::FlexStart },
        { "center", Justify::Center },
        { "flex-end", Justify::FlexEnd },
        { "end", Justify::FlexEnd },
        { "space-between", Justify::SpaceBetween },
        { "space-around", Justify::SpaceAround },
        { "space-evenly", Justify::SpaceEvenly },
    };

    inline constexpr CssKeyword<Align> AlignKeywords[] = {
        { "auto", Align::Auto },
        { "flex-start", Align::FlexStart },
        { "start", Align::FlexStart },
        { "center", Align::Center },
        { "flex-end", Align::FlexEnd },
        { "end", Align::FlexEnd },
        { "stretch", Align::Stretch },
        { "baseline", Align::Baseline },
        { "space-between", Align::SpaceBetween },
        { "space-around", Align::SpaceAround },
    };

    inline constexpr CssKeyword<FlexWrap> FlexWrapKeywords[] = {
        { "nowrap", FlexWrap::NoWrap },
        { "wrap", FlexWrap::Wrap },
    };

    inline constexpr CssKeyword<PositionType> PositionKeywords[] = {
        { "relative", PositionType::Relative },
        { "absolute", PositionType::Absolute },
    };

    inline constexpr CssKeyword<Overflow> OverflowKeywords[] = {
        { "visible", Overflow::Visible },
        { "hidden", Overflow::Hidden },
    };

    inline constexpr CssKeyword<TextAlign> TextAlignKeywords[] = {
        { "left", TextAlign::Left },
        { "center", TextAlign::Center },
        { "right", TextAlign::Right },
    };

    inline constexpr CssKeyword<PseudoClass> PseudoClassKeywords[] = {
        { "hover", PseudoClass::Hover },
        { "active", PseudoClass::Active },
        { "focus", PseudoClass::Focus },
        { "disabled", PseudoClass::Disabled },
        { "checked", PseudoClass::Checked },
    };

    template <typename Value>
    std::optional<Value> ParseKeyword(std::span<const CssKeyword<Value>> keywords, std::string_view text)
    {
        for (const CssKeyword<Value>& keyword : keywords)
        {
            if (keyword.name == text)
            {
                return keyword.value;
            }
        }
        return std::nullopt;
    }
}
