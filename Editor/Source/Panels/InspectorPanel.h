#pragma once

#include "Core/Guid.h"
#include "Framework/LevelGameplaySettings.h"
#include "Reflection/EngineObject.h"
#include "Serialization/Json.h"

#include <memory>
#include <string>

namespace BrightBox3D::Reflection
{
    class EngineObject;
}

namespace BrightBox3D::Level
{
    class LevelObjectComponent;
    class LevelObject;
}

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class InspectorPanel
    {
    public:
        explicit InspectorPanel(EditorApplication& application);
        ~InspectorPanel();

        void Draw();
        static bool DrawGameplaySettings(Serialization::JsonDocument& levelSettings, Framework::GameplaySlot slot, bool readOnly);

    private:
        void DrawObject(Level::LevelObject& object);
        bool DrawComponent(Level::LevelObject& object, Level::LevelObjectComponent& component, size_t index, size_t count, bool structural);
        void DrawAddComponent(Level::LevelObject& object);
        void DrawAsset(const Core::Guid& guid);
        void DrawImportSettings(const Core::Guid& guid);
        void DrawCollection();
        void DrawLevel();
        void TrackEdit(Level::LevelObject& object, bool changed);

        EditorApplication& m_application;
        Core::Guid m_cachedObject;
        uint64_t m_cachedRevision = 0;
        Serialization::JsonDocument m_before;
        bool m_editInProgress = false;
        std::string m_componentFilter;
        Core::Guid m_importSettingsAsset;
        std::unique_ptr<Reflection::EngineObject> m_importSettings;
        bool m_importSettingsDirty = false;
        bool m_materialEditing = false;
    };
}
