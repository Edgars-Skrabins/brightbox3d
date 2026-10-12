#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace BrightBox3D::Core
{
    std::vector<std::string> Split(std::string_view text, char separator);
    std::string Trim(std::string_view text);
    std::string ToLower(std::string_view text);
    bool StartsWith(std::string_view text, std::string_view prefix);
    bool EndsWith(std::string_view text, std::string_view suffix);
    std::string ReplaceAll(std::string text, std::string_view from, std::string_view to);
    std::string PrettifyName(std::string_view name);
}
