#include "UI/Markup.h"

#include <cstdlib>
#include <iterator>
#include <string>

namespace BrightBox3D::UI
{
    namespace
    {
        const MarkupElementInfo Elements[] = {
            { "Screen", ElementType::Panel },
            { "Widget", ElementType::Panel },
            { "Panel", ElementType::Panel },
            { "Use", ElementType::Panel },
            { "Style", ElementType::Panel },
            { "Label", ElementType::Label },
            { "Button", ElementType::Button },
            { "Image", ElementType::Image },
            { "TextField", ElementType::TextField },
            { "ProgressBar", ElementType::ProgressBar },
            { "Slider", ElementType::Slider },
            { "Toggle", ElementType::Toggle },
        };

        static_assert(std::size(Elements) == static_cast<size_t>(MarkupElement::Count));

        const char* Attributes[] = {
            "name",
            "id",
            "class",
            "style",
            "text",
            "placeholder",
            "min",
            "max",
            "value",
            "checked",
            "disabled",
            "visible",
            "src",
            "controller",
            "widget",
            "onClick",
        };

        static_assert(std::size(Attributes) == static_cast<size_t>(MarkupAttribute::Count));

        struct BindingAttribute
        {
            MarkupAttribute attribute;
            BindingTarget target;
        };

        constexpr BindingAttribute BindingAttributes[] = {
            { MarkupAttribute::Text, BindingTarget::Text },
            { MarkupAttribute::Value, BindingTarget::Value },
            { MarkupAttribute::Max, BindingTarget::Max },
            { MarkupAttribute::Checked, BindingTarget::Checked },
            { MarkupAttribute::Visible, BindingTarget::Visible },
            { MarkupAttribute::Disabled, BindingTarget::Disabled },
            { MarkupAttribute::OnClick, BindingTarget::OnClick },
        };

        static_assert(std::size(BindingAttributes) == static_cast<size_t>(BindingTarget::Count));
    }

    float ParseMarkupNumber(std::string_view text)
    {
        return std::strtof(std::string(text).c_str(), nullptr);
    }

    bool ParseMarkupBool(std::string_view text)
    {
        return text == MarkupTrue;
    }

    std::optional<MarkupElement> FindMarkupElement(std::string_view name)
    {
        for (size_t index = 0; index < std::size(Elements); ++index)
        {
            if (name == Elements[index].name)
            {
                return static_cast<MarkupElement>(index);
            }
        }
        return std::nullopt;
    }

    const MarkupElementInfo& GetMarkupElementInfo(MarkupElement element)
    {
        return Elements[static_cast<size_t>(element)];
    }

    std::optional<MarkupAttribute> FindMarkupAttribute(std::string_view name)
    {
        for (size_t index = 0; index < std::size(Attributes); ++index)
        {
            if (name == Attributes[index])
            {
                return static_cast<MarkupAttribute>(index);
            }
        }
        return std::nullopt;
    }

    const char* MarkupAttributeName(MarkupAttribute attribute)
    {
        return Attributes[static_cast<size_t>(attribute)];
    }

    std::optional<BindingTarget> BindingTargetFor(MarkupAttribute attribute)
    {
        for (const BindingAttribute& binding : BindingAttributes)
        {
            if (binding.attribute == attribute)
            {
                return binding.target;
            }
        }
        return std::nullopt;
    }
}
