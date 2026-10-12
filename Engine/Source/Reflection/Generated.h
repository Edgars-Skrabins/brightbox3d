#pragma once

#include "Core/StringUtils.h"
#include "Reflection/Object.h"
#include "Reflection/PropertyTraits.h"
#include "Reflection/TypeRegistry.h"

#include <type_traits>

namespace BrightBox3D::Reflection
{
    template <typename T>
    PropertyInfo MakeProperty(const char* name, uint32_t flags, const char* category, void* (*access)(Object*))
    {
        PropertyInfo property;
        property.name = name;
        property.displayName = Core::PrettifyName(name);
        property.category = category;
        property.flags = flags;
        property.access = access;
        DescribeProperty<std::remove_cv_t<T>>(property);
        return property;
    }

    template <typename T>
    Object* CreateObject()
    {
        return new T();
    }
}
