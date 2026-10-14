#pragma once

namespace BrightBox3D::Level
{
    class LevelCollection;
    class LevelObject;
}

namespace BrightBox3D::Editor
{
    class EditorApplication;
}

namespace BrightBox3D::Editor::LevelContentsContextMenu
{
    void DrawCreateMenu(EditorApplication& application, Level::LevelObject* parent);
    void DrawObjectMenu(EditorApplication& application, Level::LevelObject& object);
    void DrawCollectionMenu(EditorApplication& application, Level::LevelCollection& collection);
}
