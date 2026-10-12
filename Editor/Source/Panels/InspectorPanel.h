#pragma once

#include "Core/Guid.h"
#include "Serialization/Json.h"

#include <memory>
#include <string>

namespace BrightBox3D::Reflection
{
    class Object;
}

namespace BrightBox3D::Level
{
    class Component;
    class LevelObjectInstance;
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
        static bool DrawGameModeSettings(Serialization::JsonValue& settings, Serialization::JsonAllocator& allocator, bool readOnly);

    private:
        void DrawObject(Level::LevelObjectInstance& object);
        bool DrawComponent(Level::LevelObjectInstance& object, Level::Component& component, size_t index, size_t count, bool structural);
        void DrawAddComponent(Level::LevelObjectInstance& object);
        void DrawAsset(const Core::Guid& guid);
        void DrawImportSettings(const Core::Guid& guid);
        void DrawCollection();
        void DrawLevel();
        void TrackEdit(Level::LevelObjectInstance& object, bool changed);

        EditorApplication& m_application;
        Core::Guid m_cachedObject;
        uint64_t m_cachedRevision = 0;
        Serialization::JsonDocument m_before;
        bool m_editInProgress = false;
        std::string m_componentFilter;
        Core::Guid m_importSettingsAsset;
        Serialization::JsonDocument m_importSettings;
        bool m_importSettingsDirty = false;
        bool m_materialEditing = false;
    };
}
