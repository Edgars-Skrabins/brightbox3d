#include "Scripts/ScriptTemplate.h"

#include "Core/TemplateFile.h"
#include "EditorContent.h"
#include "EditorLayout.h"
#include "Framework/GameModeBase.h"
#include "Framework/GameStateBase.h"
#include "Framework/PlayableLevelObject.h"
#include "Framework/PlayerController.h"
#include "Level/LevelBehavior.h"
#include "Level/LevelObject.h"
#include "Level/LevelObjectComponent.h"
#include "ObjectIcons.h"
#include "Services/ScriptLibrary.h"

#include <iterator>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr std::string_view ClassPlaceholder = "CLASS";

        template <typename T>
        const Reflection::TypeInfo& BaseType()
        {
            return T::StaticType();
        }

        const ScriptTemplateInfo ScriptTemplates[] = {
            { "Component", EditorText::ScriptTemplateComponent, &BaseType<Level::LevelObjectComponent> },
            { "LevelObject", EditorText::ScriptTemplateLevelObject, &BaseType<Level::LevelObject> },
            { "Character", EditorText::ScriptTemplateCharacter, &BaseType<Framework::PlayableLevelObject> },
            { "GameMode", EditorText::ScriptTemplateGameMode, &BaseType<Framework::GameModeBase> },
            { "GameState", EditorText::ScriptTemplateGameState, &BaseType<Framework::GameStateBase> },
            { "PlayerController", EditorText::ScriptTemplatePlayerController, &BaseType<Framework::PlayerController> },
            { "LevelBehavior", EditorText::ScriptTemplateLevelBehavior, &BaseType<Level::LevelBehavior> }
        };

        static_assert(std::size(ScriptTemplates) == static_cast<size_t>(ScriptTemplate::Count));

        Core::Result<std::string, EditorError> RenderFile(const ScriptTemplateInfo& info, const char* extension, const std::string& className)
        {
            std::string fileName = std::string(info.fileStem) + extension + EditorLayout::TemplateFileSuffix;
            const Core::TemplateValue values[] = { { ClassPlaceholder, className } };
            std::optional<std::string> text = Core::RenderTemplate(EditorContentDirectory() / EditorLayout::ScriptTemplatesFolder / fileName, values);
            if (!text)
            {
                return EditorError(EditorText::ScriptTemplateMissing, fileName);
            }
            return *text;
        }
    }

    const ScriptTemplateInfo& GetScriptTemplateInfo(ScriptTemplate scriptTemplate)
    {
        return ScriptTemplates[static_cast<size_t>(scriptTemplate)];
    }

    Icon ScriptTemplateIcon(ScriptTemplate scriptTemplate)
    {
        Icon icon = ObjectIcons::TypeIcon(GetScriptTemplateInfo(scriptTemplate).baseType());
        return icon == Icon::None ? Icon::FileCode : icon;
    }

    Core::Result<ScriptSources, EditorError> RenderScriptTemplate(ScriptTemplate scriptTemplate, const std::string& className)
    {
        const ScriptTemplateInfo& info = GetScriptTemplateInfo(scriptTemplate);
        Core::Result<std::string, EditorError> header = RenderFile(info, ScriptLibrary::HeaderExtension, className);
        if (!header)
        {
            return header.GetError();
        }
        Core::Result<std::string, EditorError> source = RenderFile(info, ScriptLibrary::SourceExtension, className);
        if (!source)
        {
            return source.GetError();
        }
        return ScriptSources{ std::move(*header), std::move(*source) };
    }
}
