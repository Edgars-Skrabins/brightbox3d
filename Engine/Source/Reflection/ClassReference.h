#pragma once

#include "Reflection/TypeInfo.h"
#include "Reflection/TypeRegistry.h"

#include <string>

namespace BrightBox3D::Reflection
{
    template <typename T>
    struct ClassReference
    {
        static constexpr PropertyKind ReflectionKind = PropertyKind::ClassReference;
        static const TypeInfo& ReflectionBaseType() { return T::StaticType(); }

        std::string typeName = T::StaticType().GetName();

        template <typename U>
        void Set()
        {
            static_assert(std::is_base_of_v<T, U>, "ClassReference can only name subclasses of its base type");
            typeName = U::StaticType().GetName();
        }

        const TypeInfo* Resolve() const
        {
            const TypeInfo* type = TypeRegistry::Find(typeName);
            return type && type->IsA(T::StaticType()) && !type->IsAbstract() ? type : nullptr;
        }
    };
}
