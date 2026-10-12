#pragma once

#include "Reflection/TypeInfo.h"

#include <cstdint>
#include <string>
#include <vector>

namespace BrightBox3D::Reflection
{
    class TypeRegistry
    {
    public:
        static void Register(const TypeInfo& type);
        static void Unregister(const TypeInfo& type);
        static void RegisterEnum(const EnumInfo& info);
        static void UnregisterEnum(const EnumInfo& info);

        static const TypeInfo* Find(const std::string& name);
        static const EnumInfo* FindEnum(const std::string& name);
        static std::vector<const TypeInfo*> GetAll();
        static std::vector<const TypeInfo*> GetDerived(const TypeInfo& base, bool includeAbstract = false);
        static std::vector<const TypeInfo*> GetFromModule(const std::string& moduleName);
        static uint64_t Revision();

        static void SetCurrentModule(const std::string& moduleName);
        static const std::string& GetCurrentModule();
    };

    class TypeRegistration
    {
    public:
        using Getter = const TypeInfo& (*)();

        explicit TypeRegistration(Getter getter);
        ~TypeRegistration();

        TypeRegistration(const TypeRegistration&) = delete;
        TypeRegistration& operator=(const TypeRegistration&) = delete;

    private:
        const TypeInfo* m_type = nullptr;
    };

    class EnumRegistration
    {
    public:
        explicit EnumRegistration(const EnumInfo& info);
        ~EnumRegistration();

        EnumRegistration(const EnumRegistration&) = delete;
        EnumRegistration& operator=(const EnumRegistration&) = delete;

    private:
        const EnumInfo* m_info = nullptr;
    };
}
