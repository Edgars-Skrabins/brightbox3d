#pragma once

#include "Math/Color.h"
#include "UI/Style.h"

#include <array>
#include <optional>
#include <string_view>

namespace BrightBox3D::UI
{
    float ParseNumber(std::string_view text);
    Length ParseLength(std::string_view text);
    bool ParseColor(std::string_view text, Math::Color& color);
    std::optional<std::array<Length, BoxSideCount>> ParseBox(std::string_view text);
    std::string ParseFontFamily(std::string_view text);
}
