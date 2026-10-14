#include "UI/StyleResolver.h"

#include "UI/Element.h"
#include "UI/SelectorMatching.h"
#include "UI/StyleSheet.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace BrightBox3D::UI::StyleResolver
{
    namespace
    {
        using RuleMatch = std::pair<const StyleRule*, int>;

        ComputedStyle InheritedStyle(const ComputedStyle* parent)
        {
            ComputedStyle style;
            if (parent)
            {
                style.color = parent->color;
                style.fontSize = parent->fontSize;
                style.fontFamily = parent->fontFamily;
                style.textAlign = parent->textAlign;
            }
            return style;
        }

        std::vector<RuleMatch> MatchRules(const Element& element, const StyleSheet& styles)
        {
            std::vector<RuleMatch> matches;
            for (const StyleRule& rule : styles.GetRules())
            {
                int best = -1;
                for (const Selector& selector : rule.selectors)
                {
                    if (Matches(selector, element))
                    {
                        best = std::max(best, selector.specificity);
                    }
                }
                if (best >= 0)
                {
                    matches.emplace_back(&rule, best);
                }
            }
            std::stable_sort(matches.begin(), matches.end(), [](const RuleMatch& a, const RuleMatch& b)
            {
                return a.second != b.second ? a.second < b.second : a.first->order < b.first->order;
            });
            return matches;
        }

        void ApplyDeclarations(const std::vector<CssDeclaration>& declarations, ComputedStyle& style)
        {
            for (const CssDeclaration& declaration : declarations)
            {
                declaration.apply(style);
            }
        }
    }

    void ComputeStyles(Element& element, const StyleSheet& styles, const ComputedStyle* parent)
    {
        ComputedStyle style = InheritedStyle(parent);
        for (const auto& [rule, specificity] : MatchRules(element, styles))
        {
            ApplyDeclarations(rule->declarations, style);
        }
        ApplyDeclarations(element.GetInlineStyle(), style);
        if (element.IsMarkupHidden() || element.IsBindingHidden())
        {
            style.display = Display::None;
        }
        element.GetStyle() = style;
        for (const auto& child : element.GetChildren())
        {
            ComputeStyles(*child, styles, &element.GetStyle());
        }
    }
}
