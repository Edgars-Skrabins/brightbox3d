#include "UI/DocumentInput.h"

#include "Core/Utf8.h"
#include "Math/MathUtils.h"
#include "UI/BindingUpdater.h"
#include "UI/Element.h"
#include "UI/WidgetController.h"

#include <algorithm>

namespace BrightBox3D::UI
{
    namespace
    {
        constexpr char32_t FirstPrintableCodepoint = 0x20;
        constexpr float MinimumSliderWidth = 1.0f;

        Element* HitTest(Element& element, float x, float y)
        {
            if (element.GetStyle().IsHidden())
            {
                return nullptr;
            }
            bool inside = element.Contains(x, y);
            if (element.GetStyle().overflow == Overflow::Hidden && !inside)
            {
                return nullptr;
            }
            for (auto it = element.GetChildren().rbegin(); it != element.GetChildren().rend(); ++it)
            {
                if (Element* hit = HitTest(**it, x, y))
                {
                    return hit;
                }
            }
            return inside && element.IsInteractive() && !element.IsDisabled() ? &element : nullptr;
        }

        void NotifyValueChanged(Element& element)
        {
            if (WidgetController* controller = element.FindController())
            {
                controller->OnValueChanged(element);
            }
        }
    }

    void DocumentInput::Handle(Element& root, const UIInput& input, BindingUpdater& bindings)
    {
        Element* hovered = input.pointerEnabled ? HitTest(root, input.mouse.x, input.mouse.y) : nullptr;
        UpdateHover(hovered);
        if (input.mousePressed)
        {
            Press(hovered, bindings);
        }
        if (m_pressed && m_pressed->GetElementType() == ElementType::Slider && input.mouseDown)
        {
            DragSlider(input, bindings);
        }
        if (input.mouseReleased && m_pressed)
        {
            Release(hovered, bindings);
        }
        if (m_focused)
        {
            EditFocusedText(input, bindings);
        }
    }

    void DocumentInput::UpdateHover(Element* hovered)
    {
        if (m_hovered && m_hovered != hovered)
        {
            m_hovered->SetHovered(false);
        }
        m_hovered = hovered;
        if (m_hovered)
        {
            m_hovered->SetHovered(true);
        }
    }

    void DocumentInput::CommitFocus(BindingUpdater& bindings)
    {
        m_focused->SetFocused(false);
        bindings.Write(*m_focused, BindingTarget::Text);
        NotifyValueChanged(*m_focused);
        m_focused = nullptr;
    }

    void DocumentInput::Press(Element* hovered, BindingUpdater& bindings)
    {
        if (m_focused && m_focused != hovered)
        {
            CommitFocus(bindings);
        }
        if (!hovered)
        {
            return;
        }
        m_pressed = hovered;
        m_pressed->SetPressed(true);
        if (hovered->GetElementType() == ElementType::TextField)
        {
            m_focused = hovered;
            m_focused->SetFocused(true);
        }
    }

    void DocumentInput::DragSlider(const UIInput& input, BindingUpdater& bindings)
    {
        float width = std::max(m_pressed->GetWidth(), MinimumSliderWidth);
        float t = Math::Clamp01((input.mouse.x - m_pressed->GetX()) / width);
        float value = Math::Lerp(m_pressed->GetMinimum(), m_pressed->GetMaximum(), t);
        if (value != m_pressed->GetValue())
        {
            m_pressed->SetValue(value);
            bindings.Write(*m_pressed, BindingTarget::Value);
            NotifyValueChanged(*m_pressed);
        }
    }

    void DocumentInput::Release(Element* hovered, BindingUpdater& bindings)
    {
        Element* released = m_pressed;
        released->SetPressed(false);
        m_pressed = nullptr;
        if (released != hovered)
        {
            return;
        }
        if (released->GetElementType() == ElementType::Button)
        {
            bindings.Write(*released, BindingTarget::OnClick);
            if (WidgetController* controller = released->FindController())
            {
                controller->OnClicked(*released);
            }
        }
        else if (released->GetElementType() == ElementType::Toggle)
        {
            released->SetBool(!released->GetBool());
            bindings.Write(*released, BindingTarget::Checked);
            NotifyValueChanged(*released);
        }
    }

    void DocumentInput::EditFocusedText(const UIInput& input, BindingUpdater& bindings)
    {
        std::string text = m_focused->GetText();
        bool changed = false;
        for (char32_t codepoint : input.text)
        {
            if (codepoint >= FirstPrintableCodepoint)
            {
                text += Core::Utf8::Encode(codepoint);
                changed = true;
            }
        }
        if (input.backspace && !text.empty())
        {
            Core::Utf8::PopCodepoint(text);
            changed = true;
        }
        if (changed)
        {
            m_focused->SetText(text);
        }
        if (input.enter || input.escape)
        {
            CommitFocus(bindings);
        }
    }
}
