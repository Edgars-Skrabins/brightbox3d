#include "Serialization/ObjectSerializer.h"

#include "Core/EngineText.h"
#include "Core/Guid.h"
#include "Math/Color.h"
#include "Math/Quaternion.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Reflection/TypeRegistry.h"

#include <cstring>

namespace BrightBox3D::Serialization
{
    using namespace Reflection;

    namespace
    {
        void WriteFloats(const float* values, int count, JsonValue& out, JsonAllocator& allocator)
        {
            out.SetArray();
            for (int i = 0; i < count; ++i)
            {
                out.PushBack(JsonValue(FloatToJsonNumber(values[i])), allocator);
            }
        }

        bool ReadFloats(float* values, int count, const JsonValue& in)
        {
            if (!in.IsArray() || static_cast<int>(in.Size()) != count)
            {
                return false;
            }
            for (int i = 0; i < count; ++i)
            {
                if (!in[static_cast<rapidjson::SizeType>(i)].IsNumber())
                {
                    return false;
                }
            }
            for (int i = 0; i < count; ++i)
            {
                values[i] = static_cast<float>(in[static_cast<rapidjson::SizeType>(i)].GetDouble());
            }
            return true;
        }

        int64_t ReadEnumStorage(const void* value, size_t size)
        {
            switch (size)
            {
            case 1:
                return *static_cast<const int8_t*>(value);
            case 2:
                return *static_cast<const int16_t*>(value);
            case 8:
                return *static_cast<const int64_t*>(value);
            default:
                return *static_cast<const int32_t*>(value);
            }
        }

        void WriteEnumStorage(void* value, size_t size, int64_t number)
        {
            switch (size)
            {
            case 1:
                *static_cast<int8_t*>(value) = static_cast<int8_t>(number);
                break;
            case 2:
                *static_cast<int16_t*>(value) = static_cast<int16_t>(number);
                break;
            case 8:
                *static_cast<int64_t*>(value) = number;
                break;
            default:
                *static_cast<int32_t*>(value) = static_cast<int32_t>(number);
                break;
            }
        }
    }

    void WriteValue(const ValueDescriptor& descriptor, const void* value, JsonValue& out, JsonAllocator& allocator)
    {
        switch (descriptor.kind)
        {
        case PropertyKind::Bool:
            out.SetBool(*static_cast<const bool*>(value));
            break;
        case PropertyKind::Int32:
            out.SetInt(*static_cast<const int32_t*>(value));
            break;
        case PropertyKind::UInt32:
            out.SetUint(*static_cast<const uint32_t*>(value));
            break;
        case PropertyKind::Int64:
            out.SetInt64(*static_cast<const int64_t*>(value));
            break;
        case PropertyKind::Float:
            out.SetDouble(FloatToJsonNumber(*static_cast<const float*>(value)));
            break;
        case PropertyKind::Double:
            out.SetDouble(*static_cast<const double*>(value));
            break;
        case PropertyKind::String:
        case PropertyKind::ClassReference:
            out = MakeString(*static_cast<const std::string*>(value), allocator);
            break;
        case PropertyKind::Vector2:
            WriteFloats(&static_cast<const Math::Vector2*>(value)->x, 2, out, allocator);
            break;
        case PropertyKind::Vector3:
            WriteFloats(&static_cast<const Math::Vector3*>(value)->x, 3, out, allocator);
            break;
        case PropertyKind::Vector4:
            WriteFloats(&static_cast<const Math::Vector4*>(value)->x, 4, out, allocator);
            break;
        case PropertyKind::Quaternion:
            WriteFloats(&static_cast<const Math::Quaternion*>(value)->x, 4, out, allocator);
            break;
        case PropertyKind::Color:
            WriteFloats(&static_cast<const Math::Color*>(value)->r, 4, out, allocator);
            break;
        case PropertyKind::Guid:
        case PropertyKind::AssetReference:
        case PropertyKind::ObjectReference:
        {
            const Core::Guid& guid = *static_cast<const Core::Guid*>(value);
            if (guid.IsValid())
            {
                out = MakeString(guid.ToString(), allocator);
            }
            else
            {
                out.SetNull();
            }
            break;
        }
        case PropertyKind::Enum:
        {
            int64_t number = ReadEnumStorage(value, descriptor.size);
            const EnumEntry* entry = descriptor.enumInfo ? descriptor.enumInfo->FindByValue(number) : nullptr;
            if (entry)
            {
                out = MakeString(entry->name, allocator);
            }
            else
            {
                out.SetInt64(number);
            }
            break;
        }
        case PropertyKind::Array:
            out.SetNull();
            break;
        }
    }

    bool ReadValue(const ValueDescriptor& descriptor, void* value, const JsonValue& in)
    {
        switch (descriptor.kind)
        {
        case PropertyKind::Bool:
            if (!in.IsBool())
            {
                return false;
            }
            *static_cast<bool*>(value) = in.GetBool();
            return true;
        case PropertyKind::Int32:
            if (!in.IsNumber())
            {
                return false;
            }
            *static_cast<int32_t*>(value) = in.IsInt() ? in.GetInt() : static_cast<int32_t>(in.GetDouble());
            return true;
        case PropertyKind::UInt32:
            if (!in.IsNumber())
            {
                return false;
            }
            *static_cast<uint32_t*>(value) = in.IsUint() ? in.GetUint() : static_cast<uint32_t>(in.GetDouble());
            return true;
        case PropertyKind::Int64:
            if (!in.IsNumber())
            {
                return false;
            }
            *static_cast<int64_t*>(value) = in.IsInt64() ? in.GetInt64() : static_cast<int64_t>(in.GetDouble());
            return true;
        case PropertyKind::Float:
            if (!in.IsNumber())
            {
                return false;
            }
            *static_cast<float*>(value) = static_cast<float>(in.GetDouble());
            return true;
        case PropertyKind::Double:
            if (!in.IsNumber())
            {
                return false;
            }
            *static_cast<double*>(value) = in.GetDouble();
            return true;
        case PropertyKind::String:
        case PropertyKind::ClassReference:
            if (!in.IsString())
            {
                return false;
            }
            static_cast<std::string*>(value)->assign(in.GetString(), in.GetStringLength());
            return true;
        case PropertyKind::Vector2:
            return ReadFloats(&static_cast<Math::Vector2*>(value)->x, 2, in);
        case PropertyKind::Vector3:
            return ReadFloats(&static_cast<Math::Vector3*>(value)->x, 3, in);
        case PropertyKind::Vector4:
            return ReadFloats(&static_cast<Math::Vector4*>(value)->x, 4, in);
        case PropertyKind::Quaternion:
            return ReadFloats(&static_cast<Math::Quaternion*>(value)->x, 4, in);
        case PropertyKind::Color:
            return ReadFloats(&static_cast<Math::Color*>(value)->r, 4, in);
        case PropertyKind::Guid:
        case PropertyKind::AssetReference:
        case PropertyKind::ObjectReference:
        {
            Core::Guid& guid = *static_cast<Core::Guid*>(value);
            if (in.IsNull())
            {
                guid = Core::Guid{};
                return true;
            }
            if (!in.IsString())
            {
                return false;
            }
            guid = Core::Guid::Parse(in.GetString());
            return true;
        }
        case PropertyKind::Enum:
        {
            if (in.IsString())
            {
                const EnumEntry* entry = descriptor.enumInfo ? descriptor.enumInfo->FindByName(in.GetString()) : nullptr;
                if (!entry)
                {
                    return false;
                }
                WriteEnumStorage(value, descriptor.size, entry->value);
                return true;
            }
            if (in.IsInt64())
            {
                WriteEnumStorage(value, descriptor.size, in.GetInt64());
                return true;
            }
            return false;
        }
        case PropertyKind::Array:
            return false;
        }
        return false;
    }

    void WriteProperty(const PropertyInfo& property, const EngineObject& object, JsonValue& out, JsonAllocator& allocator)
    {
        const void* value = property.GetValuePointer(&object);
        if (property.value.kind != PropertyKind::Array)
        {
            WriteValue(property.value, value, out, allocator);
            return;
        }
        out.SetArray();
        size_t count = property.array.size(value);
        for (size_t i = 0; i < count; ++i)
        {
            JsonValue element;
            WriteValue(property.element, property.array.constElement(value, i), element, allocator);
            out.PushBack(element, allocator);
        }
    }

    bool ReadProperty(const PropertyInfo& property, EngineObject& object, const JsonValue& in)
    {
        void* value = property.GetValuePointer(&object);
        if (property.value.kind != PropertyKind::Array)
        {
            return ReadValue(property.value, value, in);
        }
        if (!in.IsArray())
        {
            return false;
        }
        property.array.resize(value, in.Size());
        bool success = true;
        for (rapidjson::SizeType i = 0; i < in.Size(); ++i)
        {
            success = ReadValue(property.element, property.array.element(value, i), in[i]) && success;
        }
        return success;
    }

    void SerializeProperties(const EngineObject& object, JsonValue& out, JsonAllocator& allocator)
    {
        if (!out.IsObject())
        {
            out.SetObject();
        }
        for (const PropertyInfo* property : object.GetType().GetAllProperties())
        {
            if (!property->IsSerialized())
            {
                continue;
            }
            JsonValue value;
            WriteProperty(*property, object, value, allocator);
            SetMember(out, property->name.c_str(), std::move(value), allocator);
        }
    }

    void DeserializeProperties(EngineObject& object, const JsonValue& in)
    {
        if (!in.IsObject())
        {
            return;
        }
        for (const PropertyInfo* property : object.GetType().GetAllProperties())
        {
            if (!property->IsSerialized())
            {
                continue;
            }
            const JsonValue* value = FindMember(in, property->name.c_str());
            if (value)
            {
                ReadProperty(*property, object, *value);
            }
        }
        object.OnPropertiesChanged();
    }

    void SerializeObject(const EngineObject& object, JsonValue& out, JsonAllocator& allocator)
    {
        out.SetObject();
        out.AddMember(JsonValue(TypeKey, allocator), MakeString(object.GetType().GetName(), allocator), allocator);
        SerializeProperties(object, out, allocator);
    }

    std::unique_ptr<EngineObject> DeserializeObject(const JsonValue& in, std::string* error)
    {
        std::string typeName = GetString(in, TypeKey);
        const TypeInfo* type = TypeRegistry::Find(typeName);
        if (!type)
        {
            if (error)
            {
                *error = Core::Text(Core::EngineText::UnknownType, typeName);
            }
            return nullptr;
        }
        std::unique_ptr<EngineObject> object(type->CreateInstance());
        if (!object)
        {
            if (error)
            {
                *error = Core::Text(Core::EngineText::TypeNotInstantiable, typeName);
            }
            return nullptr;
        }
        DeserializeProperties(*object, in);
        return object;
    }

    bool PropertyValuesEqual(const PropertyInfo& property, const EngineObject& a, const EngineObject& b)
    {
        JsonDocument document;
        JsonValue left;
        JsonValue right;
        WriteProperty(property, a, left, document.GetAllocator());
        WriteProperty(property, b, right, document.GetAllocator());
        return left == right;
    }

    void CopyPropertyValue(const PropertyInfo& property, const EngineObject& source, EngineObject& destination)
    {
        JsonDocument document;
        JsonValue value;
        WriteProperty(property, source, value, document.GetAllocator());
        ReadProperty(property, destination, value);
        destination.OnPropertiesChanged();
    }

    void CopyProperties(const EngineObject& source, EngineObject& destination)
    {
        JsonDocument document;
        JsonValue value;
        SerializeProperties(source, value, document.GetAllocator());
        DeserializeProperties(destination, value);
    }
}
