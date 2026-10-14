#pragma once

#include "Reflection/EngineObject.h"
#include "Serialization/Json.h"

#include <memory>
#include <string>

namespace BrightBox3D::Serialization
{
    void WriteValue(const Reflection::ValueDescriptor& descriptor, const void* value, JsonValue& out, JsonAllocator& allocator);
    bool ReadValue(const Reflection::ValueDescriptor& descriptor, void* value, const JsonValue& in);

    void WriteProperty(const Reflection::PropertyInfo& property, const Reflection::EngineObject& object, JsonValue& out, JsonAllocator& allocator);
    bool ReadProperty(const Reflection::PropertyInfo& property, Reflection::EngineObject& object, const JsonValue& in);

    void SerializeProperties(const Reflection::EngineObject& object, JsonValue& out, JsonAllocator& allocator);
    void DeserializeProperties(Reflection::EngineObject& object, const JsonValue& in);

    void SerializeObject(const Reflection::EngineObject& object, JsonValue& out, JsonAllocator& allocator);
    std::unique_ptr<Reflection::EngineObject> DeserializeObject(const JsonValue& in, std::string* error = nullptr);

    bool PropertyValuesEqual(const Reflection::PropertyInfo& property, const Reflection::EngineObject& a, const Reflection::EngineObject& b);
    void CopyPropertyValue(const Reflection::PropertyInfo& property, const Reflection::EngineObject& source, Reflection::EngineObject& destination);
    void CopyProperties(const Reflection::EngineObject& source, Reflection::EngineObject& destination);

    inline constexpr char TypeKey[] = "$type";
}
