#include "Core/TemplateFile.h"

#include "Core/FileSystem.h"
#include "Core/StringUtils.h"

namespace BrightBox3D::Core
{
    namespace
    {
        constexpr char PlaceholderMark = '@';
    }

    std::string RenderTemplateText(std::string_view text, std::span<const TemplateValue> values)
    {
        std::string result(text);
        for (const TemplateValue& value : values)
        {
            result = ReplaceAll(std::move(result), PlaceholderMark + std::string(value.name) + PlaceholderMark, value.value);
        }
        return result;
    }

    std::optional<std::string> RenderTemplate(const std::filesystem::path& path, std::span<const TemplateValue> values)
    {
        std::optional<std::string> text = FileSystem::ReadText(path);
        if (!text)
        {
            return std::nullopt;
        }
        return RenderTemplateText(*text, values);
    }
}
