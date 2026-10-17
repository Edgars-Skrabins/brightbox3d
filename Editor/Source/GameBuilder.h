#pragma once

#include "EditorText.h"

#include <filesystem>
#include <string>

namespace BrightBox3D::Editor
{
    class EditorContext;

    class GameBuilder
    {
    public:
        static std::filesystem::path DefaultOutput(EditorContext& context);
        static Core::Result<void, EditorError> Build(EditorContext& context, const std::filesystem::path& output);
    };
}
