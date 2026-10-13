#pragma once

#include "Reflection/Metadata.h"

#define BRIGHT_REFLECTION_METADATA(MetadataType, ...) \
    static_assert(::BrightBox3D::Reflection::Validate([]() { using namespace ::BrightBox3D::Reflection::Specifiers; return ::BrightBox3D::Reflection::MetadataType{ __VA_ARGS__ }; }()), "Invalid reflection metadata")

#define BrightClass(...) BRIGHT_REFLECTION_METADATA(ClassMetadata, __VA_ARGS__);
#define BrightEnum(...) BRIGHT_REFLECTION_METADATA(EnumMetadata, __VA_ARGS__);
#define BrightProperty(...) BRIGHT_REFLECTION_METADATA(PropertyMetadata, __VA_ARGS__);
#define BrightUIProperty(...) BRIGHT_REFLECTION_METADATA(UIPropertyMetadata, __VA_ARGS__);

#define BrightBody() \
public: \
    static const ::BrightBox3D::Reflection::TypeInfo& StaticType(); \
    const ::BrightBox3D::Reflection::TypeInfo& GetType() const override; \
    static ::BrightBox3D::Reflection::ClassMetadata BrightClassMetadata(); \
    static void BrightRegisterProperties(::BrightBox3D::Reflection::TypeInfo& type); \
    template <typename> \
    friend struct ::BrightBox3D::Reflection::TypeBuilder; \
\
private:
