#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace BrightBox3D::Core::Utf8
{
    std::string Encode(char32_t codepoint);
    std::vector<uint32_t> Decode(std::string_view text);
    void PopCodepoint(std::string& text);
}
