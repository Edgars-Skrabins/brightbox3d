#pragma once

#include "Core/EngineText.h"

#include <format>
#include <string>
#include <vector>

namespace BrightBox3D::Core
{
    template <typename TextId>
    class TextError
    {
    public:
        template <typename... Args>
        explicit TextError(TextId id, const Args&... arguments) : m_id(id), m_arguments{ std::format("{}", arguments)... }, m_text(Text(id, arguments...))
        {
        }

        TextId GetId() const { return m_id; }
        const std::vector<std::string>& GetArguments() const { return m_arguments; }
        const std::string& GetText() const { return m_text; }

    private:
        TextId m_id;
        std::vector<std::string> m_arguments;
        std::string m_text;
    };

    using Error = TextError<EngineText>;
}
