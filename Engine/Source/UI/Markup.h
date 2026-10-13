#pragma once

#include <optional>
#include <string_view>

namespace BrightBox3D::UI
{
    enum class ElementType
    {
        Panel,
        Label,
        Button,
        Image,
        TextField,
        ProgressBar,
        Slider,
        Toggle
    };

    enum class MarkupElement
    {
        Screen,
        Widget,
        Panel,
        Use,
        Style,
        Label,
        Button,
        Image,
        TextField,
        ProgressBar,
        Slider,
        Toggle,
        Count
    };

    struct MarkupElementInfo
    {
        const char* name;
        ElementType type;
    };

    enum class MarkupAttribute
    {
        Name,
        Id,
        Class,
        Style,
        Text,
        Placeholder,
        Min,
        Max,
        Value,
        Checked,
        Disabled,
        Visible,
        Src,
        Controller,
        Widget,
        OnClick,
        Count
    };

    enum class BindingTarget
    {
        Text,
        Value,
        Max,
        Checked,
        Visible,
        Disabled,
        OnClick,
        Count
    };

    inline constexpr std::string_view MarkupTrue = "true";
    inline constexpr std::string_view MarkupFalse = "false";

    float ParseMarkupNumber(std::string_view text);
    bool ParseMarkupBool(std::string_view text);
    std::optional<MarkupElement> FindMarkupElement(std::string_view name);
    const MarkupElementInfo& GetMarkupElementInfo(MarkupElement element);
    std::optional<MarkupAttribute> FindMarkupAttribute(std::string_view name);
    const char* MarkupAttributeName(MarkupAttribute attribute);
    std::optional<BindingTarget> BindingTargetFor(MarkupAttribute attribute);
}
