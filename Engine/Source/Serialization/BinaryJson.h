#pragma once

#include "Serialization/Json.h"

#include <cstdint>
#include <vector>

namespace BrightBox3D::Serialization
{
    std::vector<uint8_t> EncodeBinaryJson(const JsonValue& value);
    bool DecodeBinaryJson(const uint8_t* data, size_t size, JsonDocument& document);
}
