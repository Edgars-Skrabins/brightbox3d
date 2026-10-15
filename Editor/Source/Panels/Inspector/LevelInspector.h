#pragma once

#include "Framework/LevelGameplaySettings.h"
#include "Serialization/Json.h"

namespace BrightBox3D::Level
{
    class LevelCollection;
}

namespace BrightBox3D::Editor
{
    class EditorContext;
}

namespace BrightBox3D::Editor::LevelInspector
{
    void DrawLevel(EditorContext& context);
    void DrawCollection(EditorContext& context, Level::LevelCollection* collection);
    bool DrawGameplaySettings(Serialization::JsonDocument& levelSettings, Framework::GameplaySlot slot, bool readOnly);
}
