#pragma once

#include <rapidjson/document.h>

#include <filesystem>
#include <optional>
#include <string>

namespace BrightBox3D::Serialization
{
    using JsonDocument = rapidjson::Document;
    using JsonValue = rapidjson::Value;
    using JsonAllocator = rapidjson::Document::AllocatorType;

    enum class JsonFormat
    {
        Pretty,
        Compact
    };

    bool ParseJson(const std::string& text, JsonDocument& document, std::string* error = nullptr);
    bool LoadJsonFile(const std::filesystem::path& path, JsonDocument& document, std::string* error = nullptr);
    std::string WriteJson(const JsonValue& value, JsonFormat format = JsonFormat::Pretty);
    bool SaveJsonFile(const std::filesystem::path& path, const JsonValue& value);

    JsonValue MakeString(const std::string& text, JsonAllocator& allocator);
    double FloatToJsonNumber(float value);

    const JsonValue* FindMember(const JsonValue& object, const char* name);
    std::string GetString(const JsonValue& object, const char* name, const std::string& fallback = {});
    double GetNumber(const JsonValue& object, const char* name, double fallback = 0.0);
    bool GetBool(const JsonValue& object, const char* name, bool fallback = false);
    void SetMember(JsonValue& object, const char* name, JsonValue value, JsonAllocator& allocator);
}
