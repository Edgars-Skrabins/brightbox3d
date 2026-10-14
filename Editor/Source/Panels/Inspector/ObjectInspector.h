#pragma once

#include "Core/Guid.h"
#include "Panels/Inspector/ComponentInspector.h"
#include "Serialization/Json.h"

#include <cstdint>

namespace BrightBox3D::Level
{
    class LevelObject;
}

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class ObjectInspector
    {
    public:
        explicit ObjectInspector(EditorApplication& application);

        void Draw(Level::LevelObject& object);

    private:
        void RefreshSnapshot(Level::LevelObject& object);
        bool DrawHeader(Level::LevelObject& object);
        bool DrawLinkBar(Level::LevelObject& object);
        void DrawKeepChanges(Level::LevelObject& object);
        bool DrawObjectProperties(Level::LevelObject& object);
        bool DrawComponents(Level::LevelObject& object, bool structural);
        void TrackEdit(Level::LevelObject& object, bool changed);
        void RecordEdit(Level::LevelObject& object);

        EditorApplication& m_application;
        ComponentInspector m_components;
        Core::Guid m_cachedObject;
        uint64_t m_cachedRevision = 0;
        Serialization::JsonDocument m_before;
        bool m_editInProgress = false;
    };
}
