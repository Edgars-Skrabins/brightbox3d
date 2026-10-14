#pragma once

#include "Math/Vector2.h"

#include <string>

namespace BrightBox3D::UI
{
    class BindingUpdater;
    class Element;

    struct UIInput
    {
        Math::Vector2 mouse{ -1.0f, -1.0f };
        bool mouseDown = false;
        bool mousePressed = false;
        bool mouseReleased = false;
        bool pointerEnabled = true;
        std::u32string text;
        bool backspace = false;
        bool enter = false;
        bool escape = false;
    };

    class DocumentInput
    {
    public:
        void Handle(Element& root, const UIInput& input, BindingUpdater& bindings);

        bool IsPointerOverInteractive() const { return m_hovered != nullptr; }
        bool HasFocus() const { return m_focused != nullptr; }

    private:
        void UpdateHover(Element* hovered);
        void Press(Element* hovered, BindingUpdater& bindings);
        void DragSlider(const UIInput& input, BindingUpdater& bindings);
        void Release(Element* hovered, BindingUpdater& bindings);
        void EditFocusedText(const UIInput& input, BindingUpdater& bindings);
        void CommitFocus(BindingUpdater& bindings);

        Element* m_hovered = nullptr;
        Element* m_pressed = nullptr;
        Element* m_focused = nullptr;
    };
}
