#pragma once

#include <filesystem>
#include <string>

namespace BrightBox3D::Editor
{
    class EditorContext;

    class GameBuilder
    {
    public:
        static std::filesystem::path DefaultOutput(EditorContext& context);
        static bool Build(EditorContext& context, const std::filesystem::path& output, std::string& error);
    };
}
