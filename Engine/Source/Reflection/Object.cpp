#include "Reflection/Object.h"

#include "Reflection/TypeRegistry.h"

namespace BrightBox3D::Reflection
{
    const TypeInfo& Object::StaticType()
    {
        static TypeInfo type("Object", nullptr, sizeof(Object), TypeFlagAbstract, nullptr, nullptr);
        return type;
    }

    const TypeInfo& Object::GetType() const
    {
        return StaticType();
    }

    namespace
    {
        TypeRegistration s_objectRegistration(&Object::StaticType);
    }
}
