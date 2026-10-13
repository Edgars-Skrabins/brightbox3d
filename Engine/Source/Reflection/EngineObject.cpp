#include "Reflection/EngineObject.h"

#include "Reflection/TypeRegistry.h"

namespace BrightBox3D::Reflection
{
    const TypeInfo& EngineObject::StaticType()
    {
        static TypeInfo type("EngineObject", BRIGHT_MODULE_NAME, nullptr, sizeof(EngineObject), TypeFlagAbstract, nullptr, nullptr);
        return type;
    }

    const TypeInfo& EngineObject::GetType() const
    {
        return StaticType();
    }

    namespace
    {
        TypeRegistration s_objectRegistration(&EngineObject::StaticType);
    }
}
