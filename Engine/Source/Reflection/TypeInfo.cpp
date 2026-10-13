#include "Reflection/TypeInfo.h"

#include "Reflection/EngineObject.h"

namespace BrightBox3D::Reflection
{
    const EnumEntry* EnumInfo::FindByName(const std::string& entryName) const
    {
        for (const EnumEntry& entry : entries)
        {
            if (entry.name == entryName)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    const EnumEntry* EnumInfo::FindByValue(int64_t entryValue) const
    {
        for (const EnumEntry& entry : entries)
        {
            if (entry.value == entryValue)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    TypeInfo::TypeInfo(std::string name, std::string moduleName, const TypeInfo* base, size_t size, uint32_t flags, Factory factory, Builder builder)
        : m_name(std::move(name)), m_base(base), m_size(size), m_flags(flags), m_factory(factory), m_moduleName(std::move(moduleName))
    {
        if (builder)
        {
            builder(*this);
        }
    }

    bool TypeInfo::IsA(const TypeInfo& other) const
    {
        for (const TypeInfo* type = this; type; type = type->m_base)
        {
            if (type == &other)
            {
                return true;
            }
        }
        return false;
    }

    EngineObject* TypeInfo::CreateInstance() const
    {
        if (IsAbstract())
        {
            return nullptr;
        }
        return m_factory();
    }

    std::vector<const PropertyInfo*> TypeInfo::GetAllProperties() const
    {
        std::vector<const TypeInfo*> chain;
        for (const TypeInfo* type = this; type; type = type->m_base)
        {
            chain.push_back(type);
        }
        std::vector<const PropertyInfo*> properties;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        {
            for (const PropertyInfo& property : (*it)->m_properties)
            {
                properties.push_back(&property);
            }
        }
        return properties;
    }

    const PropertyInfo* TypeInfo::FindProperty(const std::string& propertyName) const
    {
        for (const TypeInfo* type = this; type; type = type->m_base)
        {
            for (const PropertyInfo& property : type->m_properties)
            {
                if (property.name == propertyName)
                {
                    return &property;
                }
            }
        }
        return nullptr;
    }

    void TypeInfo::AddProperty(PropertyInfo property)
    {
        property.owner = this;
        m_properties.push_back(std::move(property));
    }
}
