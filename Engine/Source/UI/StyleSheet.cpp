#include "UI/StyleSheet.h"

#include "Core/EngineText.h"
#include "Core/StringUtils.h"
#include "UI/CssProperties.h"

#include <cctype>

namespace BrightBox3D::UI
{
    namespace
    {
        constexpr std::string_view CommentStart = "/*";
        constexpr std::string_view CommentEnd = "*/";
        constexpr char ClassPrefix = '.';
        constexpr char IdPrefix = '#';
        constexpr char PseudoPrefix = ':';
        constexpr char Universal = '*';
        constexpr int IdSpecificity = 100;
        constexpr int ClassSpecificity = 10;
        constexpr int TypeSpecificity = 1;

        std::string StripComments(std::string_view text)
        {
            std::string result;
            for (size_t i = 0; i < text.size(); ++i)
            {
                if (text.substr(i, CommentStart.size()) == CommentStart)
                {
                    size_t end = text.find(CommentEnd, i + CommentStart.size());
                    if (end == std::string_view::npos)
                    {
                        break;
                    }
                    i = end + CommentEnd.size() - 1;
                    continue;
                }
                result += text[i];
            }
            return result;
        }

        std::string UnknownPseudoClassWarning(std::string_view name)
        {
            return Core::Text(Core::EngineText::CssUnknownPseudoClass, name);
        }

        SimpleSelector ParseSimple(const std::string& text, int& specificity, std::vector<std::string>& warnings)
        {
            SimpleSelector selector;
            size_t i = 0;
            auto readName = [&]()
            {
                size_t start = i;
                while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '-' || text[i] == '_'))
                {
                    ++i;
                }
                return text.substr(start, i - start);
            };
            while (i < text.size())
            {
                char c = text[i];
                if (c == ClassPrefix)
                {
                    ++i;
                    selector.classes.push_back(readName());
                    specificity += ClassSpecificity;
                }
                else if (c == IdPrefix)
                {
                    ++i;
                    selector.id = readName();
                    specificity += IdSpecificity;
                }
                else if (c == PseudoPrefix)
                {
                    ++i;
                    std::string name = readName();
                    specificity += ClassSpecificity;
                    if (std::optional<PseudoClass> pseudo = ParseKeyword<PseudoClass>(PseudoClassKeywords, name))
                    {
                        selector.pseudo.push_back(*pseudo);
                    }
                    else
                    {
                        warnings.push_back(UnknownPseudoClassWarning(name));
                    }
                }
                else if (c == Universal)
                {
                    ++i;
                }
                else
                {
                    selector.type = readName();
                    specificity += TypeSpecificity;
                    if (selector.type.empty())
                    {
                        ++i;
                    }
                }
            }
            return selector;
        }
    }

    Core::Result<void> StyleSheet::Parse(std::string_view source)
    {
        std::string text = StripComments(source);
        size_t position = 0;
        uint32_t order = static_cast<uint32_t>(m_rules.size());
        while (true)
        {
            size_t open = text.find('{', position);
            if (open == std::string::npos)
            {
                break;
            }
            size_t close = text.find('}', open);
            if (close == std::string::npos)
            {
                return Core::Error(Core::EngineText::CssMissingBrace);
            }
            StyleRule rule;
            rule.order = order++;
            for (const std::string& selectorText : Core::Split(std::string_view(text).substr(position, open - position), ','))
            {
                std::string trimmed = Core::Trim(selectorText);
                if (trimmed.empty())
                {
                    continue;
                }
                Selector selector;
                for (const std::string& token : Core::Split(trimmed, ' '))
                {
                    if (!token.empty())
                    {
                        selector.parts.push_back(ParseSimple(token, selector.specificity, m_warnings));
                    }
                }
                if (!selector.parts.empty())
                {
                    rule.selectors.push_back(selector);
                }
            }
            rule.declarations = ParseDeclarations(std::string_view(text).substr(open + 1, close - open - 1), m_warnings);
            if (!rule.selectors.empty())
            {
                m_rules.push_back(std::move(rule));
            }
            position = close + 1;
        }
        return {};
    }

    void StyleSheet::Append(const StyleSheet& other)
    {
        uint32_t base = static_cast<uint32_t>(m_rules.size());
        for (StyleRule rule : other.m_rules)
        {
            rule.order += base;
            m_rules.push_back(std::move(rule));
        }
        m_warnings.insert(m_warnings.end(), other.m_warnings.begin(), other.m_warnings.end());
    }
}
