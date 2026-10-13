#pragma once

#include "Core/Guid.h"
#include "UI/Binding.h"
#include "UI/Markup.h"
#include "UI/Style.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct YGNode;

namespace BrightBox3D::UI
{
    class WidgetController;

    class Element
    {
    public:
        Element(ElementType type, std::string typeName);
        ~Element();

        Element(const Element&) = delete;
        Element& operator=(const Element&) = delete;

        ElementType GetElementType() const { return m_type; }
        const std::string& GetTypeName() const { return m_typeName; }
        const std::string& GetName() const { return m_name; }
        void SetName(const std::string& name) { m_name = name; }
        bool HasClass(const std::string& name) const;
        void AddClass(const std::string& name);
        void RemoveClass(const std::string& name);
        const std::vector<std::string>& GetClasses() const { return m_classes; }

        Element* GetParent() const { return m_parent; }
        const std::vector<std::unique_ptr<Element>>& GetChildren() const { return m_children; }
        Element* AddChild(std::unique_ptr<Element> child);
        Element* FindByName(const std::string& name);

        const TextTemplate* GetBinding(BindingTarget target) const;
        void SetBinding(BindingTarget target, TextTemplate binding) { m_bindings[static_cast<size_t>(target)] = std::move(binding); }
        const std::string& GetPlaceholder() const { return m_placeholder; }
        void SetPlaceholder(const std::string& placeholder) { m_placeholder = placeholder; }
        bool IsMarkupHidden() const { return m_markupHidden; }
        void SetMarkupHidden(bool hidden) { m_markupHidden = hidden; }

        const std::string& GetText() const { return m_text; }
        void SetText(const std::string& text) { m_text = text; }
        float GetValue() const { return m_value; }
        void SetValue(float value) { m_value = value; }
        float GetMinimum() const { return m_minimum; }
        float GetMaximum() const { return m_maximum; }
        void SetRange(float minimum, float maximum)
        {
            m_minimum = minimum;
            m_maximum = maximum;
        }
        bool GetBool() const { return m_checked; }
        void SetBool(bool checked) { m_checked = checked; }
        const Core::Guid& GetImage() const { return m_image; }
        void SetImage(const Core::Guid& image) { m_image = image; }

        bool IsHovered() const { return m_hovered; }
        bool IsPressed() const { return m_pressed; }
        bool IsFocused() const { return m_focused; }
        bool IsDisabled() const { return m_disabled; }
        void SetHovered(bool hovered) { m_hovered = hovered; }
        void SetPressed(bool pressed) { m_pressed = pressed; }
        void SetFocused(bool focused) { m_focused = focused; }
        void SetDisabled(bool disabled) { m_disabled = disabled; }
        bool IsInteractive() const;

        ComputedStyle& GetStyle() { return m_style; }
        const ComputedStyle& GetStyle() const { return m_style; }
        std::vector<CssDeclaration>& GetInlineStyle() { return m_inlineStyle; }
        bool IsBindingHidden() const { return m_bindingHidden; }
        void SetBindingHidden(bool hidden) { m_bindingHidden = hidden; }
        YGNode* GetNode() const { return m_node; }

        float GetX() const { return m_x; }
        float GetY() const { return m_y; }
        float GetWidth() const { return m_width; }
        float GetHeight() const { return m_height; }
        void SetLayout(float x, float y, float width, float height)
        {
            m_x = x;
            m_y = y;
            m_width = width;
            m_height = height;
        }
        bool Contains(float x, float y) const { return x >= m_x && y >= m_y && x <= m_x + m_width && y <= m_y + m_height; }

        WidgetController* GetController() const { return m_controller; }
        void SetController(WidgetController* controller) { m_controller = controller; }
        WidgetController* FindController() const;

    private:
        ElementType m_type;
        std::string m_typeName;
        std::string m_name;
        std::vector<std::string> m_classes;
        std::array<std::optional<TextTemplate>, static_cast<size_t>(BindingTarget::Count)> m_bindings;
        std::string m_placeholder;
        bool m_markupHidden = false;
        std::vector<CssDeclaration> m_inlineStyle;
        bool m_bindingHidden = false;
        Element* m_parent = nullptr;
        std::vector<std::unique_ptr<Element>> m_children;
        ComputedStyle m_style;
        YGNode* m_node = nullptr;
        std::string m_text;
        float m_value = 0.0f;
        float m_minimum = 0.0f;
        float m_maximum = 1.0f;
        bool m_checked = false;
        Core::Guid m_image;
        bool m_hovered = false;
        bool m_pressed = false;
        bool m_focused = false;
        bool m_disabled = false;
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_width = 0.0f;
        float m_height = 0.0f;
        WidgetController* m_controller = nullptr;
    };
}
