#include "Serialization/BinaryJson.h"

#include "Serialization/BinaryStream.h"

namespace BrightBox3D::Serialization
{
    namespace
    {
        enum class Tag : uint8_t
        {
            Null = 0,
            False = 1,
            True = 2,
            Int = 3,
            UInt = 4,
            Double = 5,
            String = 6,
            Array = 7,
            Object = 8
        };

        void Encode(const JsonValue& value, BinaryWriter& writer)
        {
            switch (value.GetType())
            {
            case rapidjson::kNullType:
                writer.Write(Tag::Null);
                break;
            case rapidjson::kFalseType:
                writer.Write(Tag::False);
                break;
            case rapidjson::kTrueType:
                writer.Write(Tag::True);
                break;
            case rapidjson::kNumberType:
                if (value.IsInt64())
                {
                    int64_t number = value.GetInt64();
                    writer.Write(Tag::Int);
                    writer.WriteVarUInt((static_cast<uint64_t>(number) << 1) ^ static_cast<uint64_t>(number >> 63));
                }
                else if (value.IsUint64())
                {
                    writer.Write(Tag::UInt);
                    writer.WriteVarUInt(value.GetUint64());
                }
                else
                {
                    writer.Write(Tag::Double);
                    writer.Write(value.GetDouble());
                }
                break;
            case rapidjson::kStringType:
                writer.Write(Tag::String);
                writer.WriteVarUInt(value.GetStringLength());
                writer.WriteBytes(value.GetString(), value.GetStringLength());
                break;
            case rapidjson::kArrayType:
                writer.Write(Tag::Array);
                writer.WriteVarUInt(value.Size());
                for (const JsonValue& element : value.GetArray())
                {
                    Encode(element, writer);
                }
                break;
            case rapidjson::kObjectType:
                writer.Write(Tag::Object);
                writer.WriteVarUInt(value.MemberCount());
                for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it)
                {
                    writer.WriteVarUInt(it->name.GetStringLength());
                    writer.WriteBytes(it->name.GetString(), it->name.GetStringLength());
                    Encode(it->value, writer);
                }
                break;
            }
        }

        bool Decode(BinaryReader& reader, JsonValue& value, JsonAllocator& allocator, int depth)
        {
            if (depth > 512)
            {
                return false;
            }
            Tag tag = Tag::Null;
            if (!reader.Read(tag))
            {
                return false;
            }
            switch (tag)
            {
            case Tag::Null:
                value.SetNull();
                return true;
            case Tag::False:
                value.SetBool(false);
                return true;
            case Tag::True:
                value.SetBool(true);
                return true;
            case Tag::Int:
            {
                uint64_t encoded = 0;
                if (!reader.ReadVarUInt(encoded))
                {
                    return false;
                }
                value.SetInt64(static_cast<int64_t>((encoded >> 1) ^ (~(encoded & 1) + 1)));
                return true;
            }
            case Tag::UInt:
            {
                uint64_t number = 0;
                if (!reader.ReadVarUInt(number))
                {
                    return false;
                }
                value.SetUint64(number);
                return true;
            }
            case Tag::Double:
            {
                double number = 0.0;
                if (!reader.Read(number))
                {
                    return false;
                }
                value.SetDouble(number);
                return true;
            }
            case Tag::String:
            {
                std::string text;
                if (!reader.ReadString(text))
                {
                    return false;
                }
                value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
                return true;
            }
            case Tag::Array:
            {
                uint64_t count = 0;
                if (!reader.ReadVarUInt(count) || count > reader.Remaining())
                {
                    return false;
                }
                value.SetArray();
                value.Reserve(static_cast<rapidjson::SizeType>(count), allocator);
                for (uint64_t i = 0; i < count; ++i)
                {
                    JsonValue element;
                    if (!Decode(reader, element, allocator, depth + 1))
                    {
                        return false;
                    }
                    value.PushBack(element, allocator);
                }
                return true;
            }
            case Tag::Object:
            {
                uint64_t count = 0;
                if (!reader.ReadVarUInt(count) || count > reader.Remaining())
                {
                    return false;
                }
                value.SetObject();
                for (uint64_t i = 0; i < count; ++i)
                {
                    std::string name;
                    if (!reader.ReadString(name))
                    {
                        return false;
                    }
                    JsonValue member;
                    if (!Decode(reader, member, allocator, depth + 1))
                    {
                        return false;
                    }
                    value.AddMember(JsonValue(name.c_str(), static_cast<rapidjson::SizeType>(name.size()), allocator), member, allocator);
                }
                return true;
            }
            }
            return false;
        }
    }

    std::vector<uint8_t> EncodeBinaryJson(const JsonValue& value)
    {
        BinaryWriter writer;
        Encode(value, writer);
        return std::move(writer.GetBuffer());
    }

    bool DecodeBinaryJson(const uint8_t* data, size_t size, JsonDocument& document)
    {
        BinaryReader reader(data, size);
        document.SetNull();
        return Decode(reader, document, document.GetAllocator(), 0);
    }
}
