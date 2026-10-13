#include "UI/SelectorMatching.h"

#include "UI/Element.h"

namespace BrightBox3D::UI
{
    namespace
    {
        bool MatchesPseudo(PseudoClass pseudo, const Element& element)
        {
            switch (pseudo)
            {
            case PseudoClass::Hover:
                return element.IsHovered();
            case PseudoClass::Active:
                return element.IsPressed();
            case PseudoClass::Focus:
                return element.IsFocused();
            case PseudoClass::Disabled:
                return element.IsDisabled();
            case PseudoClass::Checked:
                return element.GetBool();
            }
            return false;
        }

        bool MatchesSimple(const SimpleSelector& selector, const Element& element)
        {
            if (!selector.type.empty() && selector.type != element.GetTypeName())
            {
                return false;
            }
            if (!selector.id.empty() && selector.id != element.GetName())
            {
                return false;
            }
            for (const std::string& name : selector.classes)
            {
                if (!element.HasClass(name))
                {
                    return false;
                }
            }
            for (PseudoClass pseudo : selector.pseudo)
            {
                if (!MatchesPseudo(pseudo, element))
                {
                    return false;
                }
            }
            return true;
        }
    }

    bool Matches(const Selector& selector, const Element& element)
    {
        if (selector.parts.empty() || !MatchesSimple(selector.parts.back(), element))
        {
            return false;
        }
        const Element* ancestor = element.GetParent();
        for (int index = static_cast<int>(selector.parts.size()) - 2; index >= 0; --index)
        {
            while (ancestor && !MatchesSimple(selector.parts[static_cast<size_t>(index)], *ancestor))
            {
                ancestor = ancestor->GetParent();
            }
            if (!ancestor)
            {
                return false;
            }
            ancestor = ancestor->GetParent();
        }
        return true;
    }
}
