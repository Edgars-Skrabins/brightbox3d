#include "UI/BindingUpdater.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "UI/Element.h"

namespace BrightBox3D::UI
{
    void BindingUpdater::ReportError(const Core::Error& error)
    {
        if (m_reportedErrors.insert(error.GetText()).second)
        {
            Core::Log::Error(Core::Text(Core::EngineText::UIBindingError, error.GetText()));
        }
    }

    template <typename Apply>
    void BindingUpdater::ReadNumber(Element& element, BindingTarget target, BindingAccess access, Apply apply)
    {
        const TextTemplate* binding = element.GetBinding(target);
        if (!binding || !binding->IsSingleBinding())
        {
            return;
        }
        Core::Result<BoundProperty> bound = ResolveBinding(binding->segments[0].text, m_roots, access);
        if (!bound)
        {
            ReportError(bound.GetError());
            return;
        }
        float number = 0.0f;
        if (*bound && bound->ReadNumber(number))
        {
            apply(number);
        }
    }

    void BindingUpdater::UpdateText(Element& element)
    {
        const TextTemplate* text = element.GetBinding(BindingTarget::Text);
        bool textField = element.GetElementType() == ElementType::TextField;
        if (!text || (textField && element.IsFocused()))
        {
            return;
        }
        if (textField && text->IsSingleBinding())
        {
            Core::Result<BoundProperty> bound = ResolveBinding(text->segments[0].text, m_roots, BindingAccess::Write);
            if (!bound)
            {
                ReportError(bound.GetError());
            }
            else if (*bound)
            {
                element.SetText(bound->Format(text->segments[0].decimals));
            }
            return;
        }
        Core::Result<std::string> evaluated = text->Evaluate(m_roots);
        if (!evaluated)
        {
            ReportError(evaluated.GetError());
            return;
        }
        element.SetText(*evaluated);
    }

    void BindingUpdater::UpdateNumbers(Element& element)
    {
        if (element.GetElementType() == ElementType::ProgressBar)
        {
            ReadNumber(element, BindingTarget::Value, BindingAccess::Read, [&](float number) { element.SetValue(number); });
            ReadNumber(element, BindingTarget::Max, BindingAccess::Read, [&](float number) { element.SetRange(element.GetMinimum(), number); });
        }
        else if (element.GetElementType() == ElementType::Slider && !element.IsPressed())
        {
            ReadNumber(element, BindingTarget::Value, BindingAccess::Write, [&](float number) { element.SetValue(number); });
        }
        if (element.GetElementType() == ElementType::Toggle)
        {
            ReadNumber(element, BindingTarget::Checked, BindingAccess::Write, [&](float number) { element.SetBool(number != 0.0f); });
        }
    }

    void BindingUpdater::UpdateVisibility(Element& element)
    {
        if (!element.GetBinding(BindingTarget::Visible))
        {
            return;
        }
        bool visible = true;
        ReadNumber(element, BindingTarget::Visible, BindingAccess::Read, [&](float number) { visible = number != 0.0f; });
        element.SetBindingHidden(!visible);
        if (!visible)
        {
            element.GetStyle().display = Display::None;
        }
    }

    void BindingUpdater::Update(Element& element)
    {
        UpdateText(element);
        UpdateNumbers(element);
        UpdateVisibility(element);
        ReadNumber(element, BindingTarget::Disabled, BindingAccess::Read, [&](float number) { element.SetDisabled(number != 0.0f); });
        for (const auto& child : element.GetChildren())
        {
            Update(*child);
        }
    }

    void BindingUpdater::Write(Element& element, BindingTarget target)
    {
        const TextTemplate* binding = element.GetBinding(target);
        if (!binding || !binding->IsSingleBinding())
        {
            return;
        }
        Core::Result<BoundProperty> result = ResolveBinding(binding->segments[0].text, m_roots, BindingAccess::Write);
        if (!result)
        {
            ReportError(result.GetError());
            return;
        }
        const BoundProperty& bound = *result;
        if (!bound)
        {
            return;
        }
        switch (target)
        {
        case BindingTarget::OnClick:
            bound.Trigger();
            break;
        case BindingTarget::Checked:
            bound.WriteBool(element.GetBool());
            break;
        case BindingTarget::Value:
            bound.WriteNumber(element.GetValue());
            break;
        case BindingTarget::Text:
            bound.WriteString(element.GetText());
            break;
        default:
            break;
        }
    }
}
