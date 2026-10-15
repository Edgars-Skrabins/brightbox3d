#pragma once

#include "Core/StringUtils.h"
#include "Reflection/EngineObject.h"
#include "Reflection/PropertyTraits.h"
#include "Reflection/TypeRegistry.h"

#include <type_traits>

namespace BrightBox3D::Reflection
{
    constexpr uint32_t EditFlags(Edit edit)
    {
        switch (edit)
        {
        case Edit::Anywhere:
            return PropertyFlagEditAnywhere;
        case Edit::DefaultsOnly:
            return PropertyFlagEditDefaultsOnly;
        case Edit::InstanceOnly:
            return PropertyFlagEditInstanceOnly;
        case Edit::Visible:
            return PropertyFlagVisibleAnywhere;
        case Edit::Hidden:
            return PropertyFlagHidden;
        case Edit::None:
            break;
        }
        return PropertyFlagNone;
    }

    constexpr uint32_t UIAccessFlags(UIAccess access)
    {
        switch (access)
        {
        case UIAccess::Read:
            return PropertyFlagUIRead;
        case UIAccess::Write:
            return PropertyFlagUIRead | PropertyFlagUIWrite;
        case UIAccess::None:
            break;
        }
        return PropertyFlagNone;
    }

    template <typename T>
    PropertyInfo MakeProperty(const char* name, const PropertyMetadata& metadata, const UIPropertyMetadata& ui, void* (*access)(EngineObject*), const void* (*constAccess)(const EngineObject*))
    {
        PropertyInfo property;
        property.name = name;
        property.displayName = metadata.displayName.IsSet() ? std::string(metadata.displayName.text) : Core::PrettifyName(name);
        property.category = std::string(metadata.category.name);
        property.flags = EditFlags(metadata.edit) | UIAccessFlags(ui.access);
        property.access = access;
        property.constAccess = constAccess;
        DescribeProperty<std::remove_cv_t<T>>(property);
        return property;
    }

    template <typename T>
    EngineObject* CreateObject()
    {
        return new T();
    }

    template <typename T>
    constexpr TypeInfo::Factory FactoryFor()
    {
        if constexpr (std::is_default_constructible_v<T>)
        {
            return &CreateObject<T>;
        }
        else
        {
            return nullptr;
        }
    }
}
