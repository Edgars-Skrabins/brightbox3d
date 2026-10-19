#pragma once

#include "EditorText.h"
#include "IconGlyphs.h"

#include <string>

namespace BrightBox3D::Reflection
{
    class TypeInfo;
}

namespace BrightBox3D::Editor
{
    enum class ScriptTemplate
    {
        Component,
        LevelObject,
        Character,
        GameMode,
        GameState,
        PlayerController,
        LevelBehavior,
        Count
    };

    struct ScriptTemplateInfo
    {
        const char* fileStem;
        EditorText label;
        const Reflection::TypeInfo& (*baseType)();
    };

    struct ScriptSources
    {
        std::string header;
        std::string source;
    };

    const ScriptTemplateInfo& GetScriptTemplateInfo(ScriptTemplate scriptTemplate);
    Icon ScriptTemplateIcon(ScriptTemplate scriptTemplate);
    Core::Result<ScriptSources, EditorError> RenderScriptTemplate(ScriptTemplate scriptTemplate, const std::string& className);
    Core::Result<std::string, EditorError> RenderTypeRedirect(const std::string& className, const std::string& oldName);
}
