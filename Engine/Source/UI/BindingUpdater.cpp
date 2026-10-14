#include "UI/BindingUpdater.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "UI/Element.h"

namespace BrightBox3D::UI
{
    void BindingUpdater::ReportError(std::string& error)
    {
        if (!error.empty() && m_reportedErrors.insert(error).second)
        {
            Core::Log::Error(Core::Text(Core::EngineText::UIBindingError, error));
        }
        error.clear();
    }

    template <typename Apply>
    void BindingUpdater::ReadNumber(Element& element, BindingTarget target, BindingAccess access, Apply apply)
    {
        const TextTemplate* binding = element.GetBinding(target);
        if (!binding || !binding->IsSingleBinding())
        {
            return;
        }
        std::string error;
        BoundProperty bound = ResolveBinding(binding->segments[0].text, m_roots, access, error);
        ReportError(error);
        float number = 0.0f;
        if (bound && bound.ReadNumber(number))
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
        std::string error;
        if (textField && text->IsSingleBinding())
        {
            BoundProperty bound = ResolveBinding(text->segments[0].text, m_roots, BindingAccess::Write, error);
            if (bound)
            {
                element.SetText(bound.Format(text->segments[0].decimals));
            }
        }
        else
        {
            element.SetText(text->Evaluate(m_roots, error));
        }
        ReportError(error);
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
        std::string error;
        BoundProperty bound = ResolveBinding(binding->segments[0].text, m_roots, BindingAccess::Write, error);
        ReportError(error);
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
