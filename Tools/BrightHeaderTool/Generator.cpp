#include "Generator.h"

#include <set>
#include <sstream>

namespace BrightBox3D::HeaderTool
{
    namespace
    {
        std::string EscapeString(const std::string& text)
        {
            std::string result;
            for (char c : text)
            {
                if (c == '"' || c == '\\')
                {
                    result += '\\';
                }
                result += c;
            }
            return result;
        }

        std::string BuildPropertyFlags(const ReflectedProperty& property, std::string& category, std::vector<Diagnostic>& errors)
        {
            std::vector<std::string> flags;
            int editModes = 0;
            for (const Specifier& specifier : property.propertySpecifiers)
            {
                if (specifier.key == "EditAnywhere")
                {
                    flags.push_back("PropertyFlagEditAnywhere");
                    ++editModes;
                }
                else if (specifier.key == "EditDefaultsOnly")
                {
                    flags.push_back("PropertyFlagEditDefaultsOnly");
                    ++editModes;
                }
                else if (specifier.key == "EditInstanceOnly")
                {
                    flags.push_back("PropertyFlagEditInstanceOnly");
                    ++editModes;
                }
                else if (specifier.key == "VisibleAnywhere")
                {
                    flags.push_back("PropertyFlagVisibleAnywhere");
                    ++editModes;
                }
                else if (specifier.key == "Hidden")
                {
                    flags.push_back("PropertyFlagHidden");
                    ++editModes;
                }
                else if (specifier.key == "Category")
                {
                    if (!specifier.hasValue || specifier.value.empty())
                    {
                        errors.push_back({ property.line, "Category requires a value, e.g. Category = \"Movement\"" });
                    }
                    category = specifier.value;
                }
                else
                {
                    errors.push_back({ property.line, "Unknown BrightProperty specifier '" + specifier.key + "'" });
                }
            }
            if (editModes > 1)
            {
                errors.push_back({ property.line, "Property '" + property.name + "' combines several edit/visibility specifiers; use exactly one" });
            }

            if (property.hasUIProperty)
            {
                int permissions = 0;
                for (const Specifier& specifier : property.uiSpecifiers)
                {
                    if (specifier.key == "Read")
                    {
                        flags.push_back("PropertyFlagUIRead");
                        ++permissions;
                    }
                    else if (specifier.key == "Write")
                    {
                        flags.push_back("PropertyFlagUIRead");
                        flags.push_back("PropertyFlagUIWrite");
                        ++permissions;
                    }
                    else
                    {
                        errors.push_back({ property.line, "Unknown BrightUIProperty specifier '" + specifier.key + "'; expected Read or Write" });
                    }
                }
                if (permissions != 1)
                {
                    errors.push_back({ property.line, "BrightUIProperty on '" + property.name + "' requires exactly one permission: Read or Write" });
                }
            }

            if (flags.empty())
            {
                return "PropertyFlagNone";
            }
            std::string result;
            for (const std::string& flag : flags)
            {
                if (!result.empty())
                {
                    result += " | ";
                }
                result += flag;
            }
            return result;
        }
    }

    GeneratorResult Generate(const ParseResult& parsed, const std::string& includePath, const std::string& uniqueId)
    {
        GeneratorResult result;
        if (parsed.classes.empty() && parsed.enums.empty())
        {
            return result;
        }

        std::ostringstream out;
        out << "#include \"" << includePath << "\"\n";
        out << "#include \"Reflection/Generated.h\"\n\n";

        for (const ReflectedEnum& reflected : parsed.enums)
        {
            out << "namespace BrightBox3D::Reflection\n{\n";
            out << "    template <>\n";
            out << "    const EnumInfo& StaticEnum<::" << reflected.qualifiedName << ">()\n    {\n";
            out << "        static const EnumInfo info{ \"" << reflected.name << "\", {\n";
            for (const std::string& entry : reflected.entries)
            {
                out << "            { \"" << entry << "\", static_cast<int64_t>(::" << reflected.qualifiedName << "::" << entry << ") },\n";
            }
            out << "        }, sizeof(::" << reflected.qualifiedName << ") };\n";
            out << "        return info;\n    }\n}\n\n";
        }

        for (const ReflectedClass& reflected : parsed.classes)
        {
            std::string qualified = "::" + reflected.qualifiedName;
            bool isAbstract = false;
            std::string typeCategory;
            for (const Specifier& specifier : reflected.specifiers)
            {
                if (specifier.key == "Abstract")
                {
                    isAbstract = true;
                }
                else if (specifier.key == "Category")
                {
                    typeCategory = specifier.value;
                }
                else
                {
                    result.errors.push_back({ reflected.line, "Unknown BrightClass specifier '" + specifier.key + "'" });
                }
            }

            std::set<std::string> names;
            out << "namespace BrightBox3D::Reflection\n{\n";
            out << "    template <>\n";
            out << "    struct TypeBuilder<" << qualified << ">\n    {\n";
            out << "        static void Build(TypeInfo& type)\n        {\n";
            if (!typeCategory.empty())
            {
                out << "            type.SetCategory(\"" << EscapeString(typeCategory) << "\");\n";
            }
            for (const ReflectedProperty& property : reflected.properties)
            {
                if (!names.insert(property.name).second)
                {
                    result.errors.push_back({ property.line, "Duplicate reflected property '" + property.name + "'" });
                    continue;
                }
                std::string category;
                std::string flags = BuildPropertyFlags(property, category, result.errors);
                out << "            type.AddProperty(MakeProperty<decltype(" << qualified << "::" << property.name << ")>(\""
                    << property.name << "\", " << flags << ", \"" << EscapeString(category) << "\", [](Object* object) -> void* { return &static_cast<"
                    << qualified << "*>(object)->" << property.name << "; }));\n";
            }
            out << "        }\n    };\n}\n\n";

            out << "const ::BrightBox3D::Reflection::TypeInfo& " << qualified << "::StaticType()\n{\n";
            out << "    static const ::BrightBox3D::Reflection::TypeInfo type(\"" << reflected.name << "\", &" << reflected.baseName << "::StaticType(), sizeof("
                << qualified << "), " << (isAbstract ? "::BrightBox3D::Reflection::TypeFlagAbstract" : "::BrightBox3D::Reflection::TypeFlagNone") << ", "
                << (isAbstract ? "nullptr" : "&::BrightBox3D::Reflection::CreateObject<" + qualified + ">") << ", &::BrightBox3D::Reflection::TypeBuilder<"
                << qualified << ">::Build);\n";
            out << "    return type;\n}\n\n";
            out << "const ::BrightBox3D::Reflection::TypeInfo& " << qualified << "::GetType() const\n{\n    return StaticType();\n}\n\n";
        }

        out << "namespace\n{\n";
        int index = 0;
        for (const ReflectedEnum& reflected : parsed.enums)
        {
            out << "    ::BrightBox3D::Reflection::EnumRegistration s_enumRegistration_" << uniqueId << "_" << index++
                << "(::BrightBox3D::Reflection::StaticEnum<::" << reflected.qualifiedName << ">());\n";
        }
        index = 0;
        for (const ReflectedClass& reflected : parsed.classes)
        {
            out << "    ::BrightBox3D::Reflection::TypeRegistration s_typeRegistration_" << uniqueId << "_" << index++
                << "(&::" << reflected.qualifiedName << "::StaticType);\n";
        }
        out << "}\n";

        result.code = out.str();
        return result;
    }
}
