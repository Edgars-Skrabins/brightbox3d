#include "UI/Element.h"

#include <yoga/Yoga.h>

#include <algorithm>

namespace BrightBox3D::UI
{
    Element::Element(ElementType type, std::string typeName) : m_type(type), m_typeName(std::move(typeName))
    {
        m_node = YGNodeNew();
        YGNodeSetContext(m_node, this);
    }

    Element::~Element()
    {
        m_children.clear();
        if (m_node)
        {
            YGNodeFree(m_node);
        }
    }

    bool Element::HasClass(const std::string& name) const
    {
        return std::find(m_classes.begin(), m_classes.end(), name) != m_classes.end();
    }

    void Element::AddClass(const std::string& name)
    {
        if (!name.empty() && !HasClass(name))
        {
            m_classes.push_back(name);
        }
    }

    void Element::RemoveClass(const std::string& name)
    {
        m_classes.erase(std::remove(m_classes.begin(), m_classes.end(), name), m_classes.end());
    }

    Element* Element::AddChild(std::unique_ptr<Element> child)
    {
        child->m_parent = this;
        YGNodeInsertChild(m_node, child->m_node, YGNodeGetChildCount(m_node));
        m_children.push_back(std::move(child));
        return m_children.back().get();
    }

    Element* Element::FindByName(const std::string& name)
    {
        if (m_name == name)
        {
            return this;
        }
        for (const auto& child : m_children)
        {
            if (Element* found = child->FindByName(name))
            {
                return found;
            }
        }
        return nullptr;
    }

    const TextTemplate* Element::GetBinding(BindingTarget target) const
    {
        const std::optional<TextTemplate>& binding = m_bindings[static_cast<size_t>(target)];
        return binding ? &*binding : nullptr;
    }

    bool Element::IsInteractive() const
    {
        return m_type == ElementType::Button || m_type == ElementType::TextField || m_type == ElementType::Slider || m_type == ElementType::Toggle;
    }

    bool Element::ShowsText() const
    {
        return m_type == ElementType::Label || m_type == ElementType::Button || m_type == ElementType::TextField;
    }

    const std::string& Element::GetDisplayText() const
    {
        if (m_type == ElementType::TextField && m_text.empty() && !m_focused)
        {
            return m_placeholder;
        }
        return m_text;
    }

    WidgetController* Element::FindController() const
    {
        for (const Element* element = this; element; element = element->m_parent)
        {
            if (element->m_controller)
            {
                return element->m_controller;
            }
        }
        return nullptr;
    }
}
