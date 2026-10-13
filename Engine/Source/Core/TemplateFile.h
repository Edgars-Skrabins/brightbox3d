#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace BrightBox3D::Core
{
    struct TemplateValue
    {
        std::string_view name;
        std::string value;
    };

    std::string RenderTemplateText(std::string_view text, std::span<const TemplateValue> values);
    std::optional<std::string> RenderTemplate(const std::filesystem::path& path, std::span<const TemplateValue> values);
}
