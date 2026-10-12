#pragma once

#include "Reflection/Macros.h"
#include "Reflection/TypeInfo.h"

namespace BrightBox3D::Reflection
{
    class Object
    {
    public:
        virtual ~Object() = default;

        static const TypeInfo& StaticType();
        virtual const TypeInfo& GetType() const;
        virtual void OnPropertiesChanged() {}

        bool IsA(const TypeInfo& type) const { return GetType().IsA(type); }

        template <typename T>
        bool IsA() const
        {
            return GetType().IsA(T::StaticType());
        }
    };

    template <typename T>
    T* Cast(Object* object)
    {
        return object && object->IsA<T>() ? static_cast<T*>(object) : nullptr;
    }

    template <typename T>
    const T* Cast(const Object* object)
    {
        return object && object->IsA<T>() ? static_cast<const T*>(object) : nullptr;
    }
}
