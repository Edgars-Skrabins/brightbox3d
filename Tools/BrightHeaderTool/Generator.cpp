#include "Generator.h"

#include <set>
#include <sstream>

namespace BrightBox3D::HeaderTool
{
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
            std::string reflection = "::BrightBox3D::Reflection::";

            out << "namespace BrightBox3D::Reflection\n{\n";
            out << "    template <>\n";
            out << "    struct TypeBuilder<" << qualified << ">\n    {\n";
            out << "        static void Build(TypeInfo& type)\n        {\n";
            out << "            " << qualified << "::BrightRegisterProperties(type);\n";
            out << "        }\n    };\n}\n\n";

            out << reflection << "ClassMetadata " << reflected.qualifiedName << "::BrightClassMetadata()\n{\n";
            out << "    using namespace " << reflection << "Specifiers;\n";
            out << "    return " << reflection << "ClassMetadata{ " << reflected.metadata << " };\n}\n\n";

            out << "void " << qualified << "::BrightRegisterProperties(" << reflection << "TypeInfo& type)\n{\n";
            out << "    using namespace " << reflection << "Specifiers;\n";
            out << "    " << reflection << "ClassMetadata metadata = BrightClassMetadata();\n";
            out << "    type.SetCategory(std::string(metadata.category.name));\n";
            out << "    type.SetIcon(metadata.icon);\n";
            out << "    type.SetHeaderPath(\"" << includePath << "\");\n";
            std::set<std::string> names;
            for (const ReflectedProperty& property : reflected.properties)
            {
                if (!names.insert(property.name).second)
                {
                    result.errors.push_back({ property.line, "Duplicate reflected property '" + property.name + "'" });
                    continue;
                }
                out << "    type.AddProperty(" << reflection << "MakeProperty<decltype(" << qualified << "::" << property.name << ")>(\"" << property.name << "\", "
                    << reflection << "PropertyMetadata{ " << property.propertyMetadata << " }, "
                    << reflection << "UIPropertyMetadata{ " << property.uiMetadata << " }, [](" << reflection << "EngineObject* object) -> void* { return &static_cast<"
                    << qualified << "*>(object)->" << property.name << "; }, [](const " << reflection << "EngineObject* object) -> const void* { return &static_cast<const "
                    << qualified << "*>(object)->" << property.name << "; }));\n";
            }
            out << "}\n\n";

            out << "const " << reflection << "TypeInfo& " << qualified << "::StaticType()\n{\n";
            out << "    static const bool isAbstract = BrightClassMetadata().abstract;\n";
            out << "    static const " << reflection << "TypeInfo type(\"" << reflected.name << "\", BRIGHT_MODULE_NAME, &" << reflected.baseName << "::StaticType(), sizeof(" << qualified
                << "), isAbstract ? " << reflection << "TypeFlagAbstract : " << reflection << "TypeFlagNone, isAbstract ? nullptr : " << reflection << "FactoryFor<" << qualified
                << ">(), &" << reflection << "TypeBuilder<" << qualified << ">::Build);\n";
            out << "    return type;\n}\n\n";
            out << "const " << reflection << "TypeInfo& " << qualified << "::GetType() const\n{\n    return StaticType();\n}\n\n";
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
