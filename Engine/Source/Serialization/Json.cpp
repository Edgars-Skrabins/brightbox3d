#include "Serialization/Json.h"

#include "Core/FileSystem.h"

#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <charconv>
#include <cstdlib>

namespace BrightBox3D::Serialization
{
    bool ParseJson(const std::string& text, JsonDocument& document, std::string* error)
    {
        document.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag | rapidjson::kParseNanAndInfFlag>(text.c_str(), text.size());
        if (document.HasParseError())
        {
            if (error)
            {
                size_t offset = document.GetErrorOffset();
                int line = 1;
                for (size_t i = 0; i < offset && i < text.size(); ++i)
                {
                    if (text[i] == '\n')
                    {
                        ++line;
                    }
                }
                *error = std::string(rapidjson::GetParseError_En(document.GetParseError())) + " (line " + std::to_string(line) + ")";
            }
            return false;
        }
        return true;
    }

    bool LoadJsonFile(const std::filesystem::path& path, JsonDocument& document, std::string* error)
    {
        auto text = Core::FileSystem::ReadText(path);
        if (!text)
        {
            if (error)
            {
                *error = "cannot read file";
            }
            return false;
        }
        return ParseJson(*text, document, error);
    }

    std::string WriteJson(const JsonValue& value, bool pretty)
    {
        rapidjson::StringBuffer buffer;
        if (pretty)
        {
            rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag> writer(buffer);
            writer.SetIndent(' ', 4);
            writer.SetFormatOptions(rapidjson::kFormatSingleLineArray);
            value.Accept(writer);
        }
        else
        {
            rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag> writer(buffer);
            value.Accept(writer);
        }
        std::string text(buffer.GetString(), buffer.GetSize());
        if (pretty)
        {
            text += '\n';
        }
        return text;
    }

    bool SaveJsonFile(const std::filesystem::path& path, const JsonValue& value)
    {
        return Core::FileSystem::WriteText(path, WriteJson(value, true));
    }

    JsonValue MakeString(const std::string& text, JsonAllocator& allocator)
    {
        JsonValue value;
        value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
        return value;
    }

    double FloatToJsonNumber(float value)
    {
        char buffer[64];
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        *result.ptr = '\0';
        return std::strtod(buffer, nullptr);
    }

    const JsonValue* FindMember(const JsonValue& object, const char* name)
    {
        if (!object.IsObject())
        {
            return nullptr;
        }
        auto it = object.FindMember(name);
        return it == object.MemberEnd() ? nullptr : &it->value;
    }

    std::string GetString(const JsonValue& object, const char* name, const std::string& fallback)
    {
        const JsonValue* value = FindMember(object, name);
        return value && value->IsString() ? std::string(value->GetString(), value->GetStringLength()) : fallback;
    }

    double GetNumber(const JsonValue& object, const char* name, double fallback)
    {
        const JsonValue* value = FindMember(object, name);
        return value && value->IsNumber() ? value->GetDouble() : fallback;
    }

    bool GetBool(const JsonValue& object, const char* name, bool fallback)
    {
        const JsonValue* value = FindMember(object, name);
        return value && value->IsBool() ? value->GetBool() : fallback;
    }

    void SetMember(JsonValue& object, const char* name, JsonValue value, JsonAllocator& allocator)
    {
        auto it = object.FindMember(name);
        if (it != object.MemberEnd())
        {
            it->value = std::move(value);
            return;
        }
        object.AddMember(JsonValue(name, allocator), std::move(value), allocator);
    }
}
