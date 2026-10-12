#include "Parser.h"

namespace BrightBox3D::HeaderTool
{
    namespace
    {
        enum class ScopeKind
        {
            Namespace,
            Class,
            Other
        };

        struct Scope
        {
            ScopeKind kind = ScopeKind::Other;
            std::string name;
            int classIndex = -1;
        };

        struct PendingMarker
        {
            bool active = false;
            std::vector<Specifier> specifiers;
            int line = 0;
        };

        std::string Unquote(const std::string& text)
        {
            if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
            {
                return text.substr(1, text.size() - 2);
            }
            return text;
        }

        class Parser
        {
        public:
            explicit Parser(const std::vector<Token>& tokens) : m_tokens(tokens) {}

            ParseResult Run()
            {
                while (m_index < m_tokens.size())
                {
                    Step();
                }
                if (m_pendingClass.active)
                {
                    Error(m_pendingClass.line, "BrightClass() must be followed by a class or struct definition");
                }
                if (m_pendingEnum.active)
                {
                    Error(m_pendingEnum.line, "BrightEnum() must be followed by an enum definition");
                }
                for (const ReflectedClass& reflected : m_result.classes)
                {
                    if (!reflected.hasBody)
                    {
                        Error(reflected.line, "BrightClass '" + reflected.name + "' is missing BrightBody()");
                    }
                    if (reflected.baseName.empty())
                    {
                        Error(reflected.line, "BrightClass '" + reflected.name + "' must derive from a reflected base class");
                    }
                }
                return std::move(m_result);
            }

        private:
            const Token& Current() const { return m_tokens[m_index]; }

            bool Peek(size_t offset, const char* text) const
            {
                return m_index + offset < m_tokens.size() && m_tokens[m_index + offset].text == text;
            }

            void Error(int line, const std::string& message)
            {
                m_result.errors.push_back({ line, message });
            }

            Scope* CurrentReflectedClass()
            {
                if (m_scopes.empty() || m_scopes.back().kind != ScopeKind::Class || m_scopes.back().classIndex < 0)
                {
                    return nullptr;
                }
                return &m_scopes.back();
            }

            std::string ScopePath() const
            {
                std::string path;
                for (const Scope& scope : m_scopes)
                {
                    if (scope.kind == ScopeKind::Other || scope.name.empty())
                    {
                        continue;
                    }
                    if (!path.empty())
                    {
                        path += "::";
                    }
                    path += scope.name;
                }
                return path;
            }

            std::vector<Specifier> ReadSpecifiers()
            {
                std::vector<Specifier> specifiers;
                if (!Peek(0, "("))
                {
                    return specifiers;
                }
                ++m_index;
                int depth = 0;
                std::vector<std::vector<const Token*>> groups(1);
                while (m_index < m_tokens.size())
                {
                    const Token& token = Current();
                    if (token.Is("(") || token.Is("<") || token.Is("{"))
                    {
                        ++depth;
                    }
                    else if (token.Is(")") || token.Is(">") || token.Is("}"))
                    {
                        if (depth == 0 && token.Is(")"))
                        {
                            ++m_index;
                            break;
                        }
                        --depth;
                    }
                    else if (token.Is(",") && depth == 0)
                    {
                        groups.emplace_back();
                        ++m_index;
                        continue;
                    }
                    groups.back().push_back(&token);
                    ++m_index;
                }
                for (const auto& group : groups)
                {
                    if (group.empty())
                    {
                        continue;
                    }
                    Specifier specifier;
                    specifier.key = group[0]->text;
                    if (group.size() > 1 && group[1]->Is("="))
                    {
                        specifier.hasValue = true;
                        std::string value;
                        for (size_t i = 2; i < group.size(); ++i)
                        {
                            value += group[i]->type == TokenType::String ? Unquote(group[i]->text) : group[i]->text;
                        }
                        specifier.value = value;
                    }
                    specifiers.push_back(specifier);
                }
                return specifiers;
            }

            void Step()
            {
                const Token& token = Current();
                if (token.type == TokenType::Identifier)
                {
                    if (token.Is("BrightClass"))
                    {
                        int line = token.line;
                        ++m_index;
                        m_pendingClass.active = true;
                        m_pendingClass.line = line;
                        m_pendingClass.specifiers = ReadSpecifiers();
                        return;
                    }
                    if (token.Is("BrightEnum"))
                    {
                        int line = token.line;
                        ++m_index;
                        m_pendingEnum.active = true;
                        m_pendingEnum.line = line;
                        m_pendingEnum.specifiers = ReadSpecifiers();
                        return;
                    }
                    if (token.Is("BrightBody"))
                    {
                        ++m_index;
                        ReadSpecifiers();
                        if (Scope* scope = CurrentReflectedClass())
                        {
                            m_result.classes[static_cast<size_t>(scope->classIndex)].hasBody = true;
                        }
                        else
                        {
                            Error(token.line, "BrightBody() used outside of a BrightClass");
                        }
                        return;
                    }
                    if (token.Is("BrightProperty") || token.Is("BrightUIProperty"))
                    {
                        ParseProperty();
                        return;
                    }
                    if (token.Is("namespace"))
                    {
                        ParseNamespace();
                        return;
                    }
                    if (token.Is("enum"))
                    {
                        ParseEnum();
                        return;
                    }
                    if (token.Is("class") || token.Is("struct"))
                    {
                        if (ParseClass())
                        {
                            return;
                        }
                    }
                }
                else if (token.Is("{"))
                {
                    m_scopes.push_back({ ScopeKind::Other, {}, -1 });
                }
                else if (token.Is("}"))
                {
                    if (!m_scopes.empty())
                    {
                        m_scopes.pop_back();
                    }
                }
                ++m_index;
            }

            void ParseNamespace()
            {
                ++m_index;
                std::string name;
                while (m_index < m_tokens.size() && !Peek(0, "{") && !Peek(0, ";") && !Peek(0, "="))
                {
                    name += Current().text;
                    ++m_index;
                }
                if (Peek(0, "{"))
                {
                    m_scopes.push_back({ ScopeKind::Namespace, name, -1 });
                    ++m_index;
                }
            }

            void ParseEnum()
            {
                int line = Current().line;
                ++m_index;
                if (Peek(0, "class") || Peek(0, "struct"))
                {
                    ++m_index;
                }
                std::string name;
                if (m_index < m_tokens.size() && Current().type == TokenType::Identifier)
                {
                    name = Current().text;
                }
                while (m_index < m_tokens.size() && !Peek(0, "{") && !Peek(0, ";"))
                {
                    ++m_index;
                }
                if (m_index >= m_tokens.size() || Peek(0, ";"))
                {
                    return;
                }

                ReflectedEnum reflected;
                reflected.name = name;
                std::string path = ScopePath();
                reflected.qualifiedName = path.empty() ? name : path + "::" + name;
                reflected.line = line;

                ++m_index;
                int depth = 0;
                bool expectName = true;
                while (m_index < m_tokens.size())
                {
                    const Token& token = Current();
                    if (token.Is("(") || token.Is("{") || token.Is("<"))
                    {
                        ++depth;
                    }
                    else if (token.Is(")") || token.Is(">"))
                    {
                        --depth;
                    }
                    else if (token.Is("}"))
                    {
                        if (depth == 0)
                        {
                            ++m_index;
                            break;
                        }
                        --depth;
                    }
                    else if (token.Is(",") && depth == 0)
                    {
                        expectName = true;
                    }
                    else if (expectName && depth == 0 && token.type == TokenType::Identifier)
                    {
                        reflected.entries.push_back(token.text);
                        expectName = false;
                    }
                    ++m_index;
                }

                if (m_pendingEnum.active)
                {
                    if (name.empty())
                    {
                        Error(line, "BrightEnum() requires a named enum");
                    }
                    else
                    {
                        m_result.enums.push_back(reflected);
                    }
                    m_pendingEnum = {};
                }
            }

            bool ParseClass()
            {
                size_t start = m_index;
                size_t j = m_index + 1;
                while (j < m_tokens.size() && (m_tokens[j].Is("[") || m_tokens[j].Is("alignas")))
                {
                    int depth = 0;
                    do
                    {
                        if (m_tokens[j].Is("[") || m_tokens[j].Is("("))
                        {
                            ++depth;
                        }
                        else if (m_tokens[j].Is("]") || m_tokens[j].Is(")"))
                        {
                            --depth;
                        }
                        ++j;
                    } while (j < m_tokens.size() && depth > 0);
                }
                if (j >= m_tokens.size() || m_tokens[j].type != TokenType::Identifier)
                {
                    return false;
                }
                std::string name = m_tokens[j].text;
                int line = m_tokens[start].line;
                ++j;
                if (j < m_tokens.size() && m_tokens[j].Is("final"))
                {
                    ++j;
                }

                size_t open = 0;
                bool inBaseList = false;
                std::vector<std::vector<const Token*>> bases;
                int angleDepth = 0;
                for (; j < m_tokens.size(); ++j)
                {
                    const Token& token = m_tokens[j];
                    if (token.Is("{"))
                    {
                        open = j;
                        break;
                    }
                    if (!inBaseList)
                    {
                        if (token.Is(":"))
                        {
                            inBaseList = true;
                            bases.emplace_back();
                            continue;
                        }
                        return false;
                    }
                    if (token.Is("<"))
                    {
                        ++angleDepth;
                    }
                    else if (token.Is(">"))
                    {
                        --angleDepth;
                    }
                    else if (token.Is(";"))
                    {
                        return false;
                    }
                    if (token.Is(",") && angleDepth == 0)
                    {
                        bases.emplace_back();
                        continue;
                    }
                    if (token.Is("public") || token.Is("protected") || token.Is("private") || token.Is("virtual"))
                    {
                        continue;
                    }
                    bases.back().push_back(&token);
                }
                if (open == 0)
                {
                    return false;
                }

                Scope scope{ ScopeKind::Class, name, -1 };
                if (m_pendingClass.active)
                {
                    ReflectedClass reflected;
                    reflected.name = name;
                    std::string path = ScopePath();
                    reflected.qualifiedName = path.empty() ? name : path + "::" + name;
                    reflected.specifiers = m_pendingClass.specifiers;
                    reflected.line = line;
                    if (!bases.empty())
                    {
                        for (const Token* baseToken : bases.front())
                        {
                            reflected.baseName += baseToken->text;
                        }
                    }
                    scope.classIndex = static_cast<int>(m_result.classes.size());
                    m_result.classes.push_back(reflected);
                    m_pendingClass = {};
                }
                m_scopes.push_back(scope);
                m_index = open + 1;
                return true;
            }

            void ParseProperty()
            {
                ReflectedProperty property;
                property.line = Current().line;
                bool insideClass = CurrentReflectedClass() != nullptr;

                while (m_index < m_tokens.size() && (Peek(0, "BrightProperty") || Peek(0, "BrightUIProperty")))
                {
                    bool ui = Current().Is("BrightUIProperty");
                    ++m_index;
                    std::vector<Specifier> specifiers = ReadSpecifiers();
                    if (ui)
                    {
                        if (property.hasUIProperty)
                        {
                            Error(property.line, "BrightUIProperty specified twice on the same member");
                        }
                        property.hasUIProperty = true;
                        property.uiSpecifiers = specifiers;
                    }
                    else
                    {
                        if (property.hasProperty)
                        {
                            Error(property.line, "BrightProperty specified twice on the same member");
                        }
                        property.hasProperty = true;
                        property.propertySpecifiers = specifiers;
                    }
                }

                int parenDepth = 0;
                int angleDepth = 0;
                std::string lastIdentifier;
                bool isFunction = false;
                bool found = false;
                while (m_index < m_tokens.size())
                {
                    const Token& token = Current();
                    if (token.Is("<"))
                    {
                        ++angleDepth;
                    }
                    else if (token.Is(">"))
                    {
                        --angleDepth;
                    }
                    else if (token.Is("("))
                    {
                        if (angleDepth == 0 && parenDepth == 0)
                        {
                            isFunction = true;
                        }
                        ++parenDepth;
                    }
                    else if (token.Is(")"))
                    {
                        --parenDepth;
                    }
                    else if (parenDepth == 0 && angleDepth == 0 && (token.Is("=") || token.Is("{") || token.Is(";") || token.Is("[") || token.Is(":")))
                    {
                        found = true;
                        break;
                    }
                    else if (token.type == TokenType::Identifier)
                    {
                        lastIdentifier = token.text;
                    }
                    ++m_index;
                }

                int braceDepth = 0;
                while (m_index < m_tokens.size())
                {
                    const Token& token = Current();
                    if (token.Is("{"))
                    {
                        ++braceDepth;
                    }
                    else if (token.Is("}"))
                    {
                        --braceDepth;
                    }
                    else if (token.Is(";") && braceDepth <= 0)
                    {
                        ++m_index;
                        break;
                    }
                    ++m_index;
                }

                if (!insideClass)
                {
                    Error(property.line, "BrightProperty/BrightUIProperty must be used on a member of a BrightClass");
                    return;
                }
                if (isFunction || !found || lastIdentifier.empty())
                {
                    Error(property.line, "BrightProperty/BrightUIProperty must precede a data member declaration");
                    return;
                }
                property.name = lastIdentifier;
                m_result.classes[static_cast<size_t>(CurrentReflectedClass()->classIndex)].properties.push_back(property);
            }

            const std::vector<Token>& m_tokens;
            size_t m_index = 0;
            std::vector<Scope> m_scopes;
            PendingMarker m_pendingClass;
            PendingMarker m_pendingEnum;
            ParseResult m_result;
        };
    }

    ParseResult Parse(const std::vector<Token>& tokens)
    {
        Parser parser(tokens);
        return parser.Run();
    }
}
