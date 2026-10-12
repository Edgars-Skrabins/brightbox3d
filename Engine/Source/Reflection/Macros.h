#pragma once

#define BrightClass(...)
#define BrightEnum(...)
#define BrightProperty(...)
#define BrightUIProperty(...)

#define BrightBody() \
public: \
    static const ::BrightBox3D::Reflection::TypeInfo& StaticType(); \
    const ::BrightBox3D::Reflection::TypeInfo& GetType() const override; \
    template <typename> \
    friend struct ::BrightBox3D::Reflection::TypeBuilder; \
\
private:
