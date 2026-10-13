#include "Core/TemplateFile.h"

#include "Core/FileSystem.h"

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
            std::string placeholder = PlaceholderMark + std::string(value.name) + PlaceholderMark;
            for (size_t position = result.find(placeholder); position != std::string::npos; position = result.find(placeholder, position + value.value.size()))
            {
                result.replace(position, placeholder.size(), value.value);
            }
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
