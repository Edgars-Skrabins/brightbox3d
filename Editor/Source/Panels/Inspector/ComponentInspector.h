#pragma once

#include <cstddef>
#include <string>

namespace BrightBox3D::Level
{
    class LevelObject;
    class LevelObjectComponent;
}

namespace BrightBox3D::Editor
{
    class EditorApplication;
    class EditorContext;
    struct PropertyDrawOptions;

    void AddOverrideHandlers(PropertyDrawOptions& options, EditorContext& context, Level::LevelObject& object, Level::LevelObjectComponent* component);

    class ComponentInspector
    {
    public:
        explicit ComponentInspector(EditorApplication& application);

        bool Draw(Level::LevelObject& object, Level::LevelObjectComponent& component, size_t index, size_t count, bool structural);
        bool DrawAddComponent(Level::LevelObject& object);

    private:
        bool DrawContextMenu(Level::LevelObject& object, Level::LevelObjectComponent& component, size_t index, size_t count, bool structural, bool& removed);
        bool DrawEnabledToggle(Level::LevelObjectComponent& component);
        bool DrawProperties(Level::LevelObject& object, Level::LevelObjectComponent& component);

        EditorApplication& m_application;
        std::string m_filter;
    };
}
