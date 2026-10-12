#pragma once

#include "Reflection/Object.h"
#include "Serialization/Json.h"

#include <memory>
#include <string>

namespace BrightBox3D::Serialization
{
    void WriteValue(const Reflection::ValueDescriptor& descriptor, const void* value, JsonValue& out, JsonAllocator& allocator);
    bool ReadValue(const Reflection::ValueDescriptor& descriptor, void* value, const JsonValue& in);

    void WriteProperty(const Reflection::PropertyInfo& property, const Reflection::Object& object, JsonValue& out, JsonAllocator& allocator);
    bool ReadProperty(const Reflection::PropertyInfo& property, Reflection::Object& object, const JsonValue& in);

    void SerializeProperties(const Reflection::Object& object, JsonValue& out, JsonAllocator& allocator);
    void DeserializeProperties(Reflection::Object& object, const JsonValue& in);

    void SerializeObject(const Reflection::Object& object, JsonValue& out, JsonAllocator& allocator);
    std::unique_ptr<Reflection::Object> DeserializeObject(const JsonValue& in, std::string* error = nullptr);

    bool PropertyValuesEqual(const Reflection::PropertyInfo& property, const Reflection::Object& a, const Reflection::Object& b);
    void CopyPropertyValue(const Reflection::PropertyInfo& property, const Reflection::Object& source, Reflection::Object& destination);
    void CopyProperties(const Reflection::Object& source, Reflection::Object& destination);

    inline constexpr const char* TypeKey = "$type";
}
