#pragma once

#include "Core/Guid.h"
#include "ObjectIcons.h"

#include <string>

namespace BrightBox3D::Level
{
    class LevelCollection;
    class LevelObject;
}

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class LevelContentsPanel
    {
    public:
        explicit LevelContentsPanel(EditorApplication& application);
        void Draw();

    private:
        void DrawCollection(Level::LevelCollection& collection);
        void DrawObject(Level::LevelObject& object);
        void DrawCreateMenu(Level::LevelObject* parent);
        void DrawObjectContextMenu(Level::LevelObject& object);
        bool MatchesFilter(const Level::LevelObject& object) const;
        void DrawRowBackground(bool selected);

        EditorApplication& m_application;
        std::string m_filter;
        Core::Guid m_renaming;
        std::string m_renameBuffer;
        bool m_focusRename = false;
        Core::Guid m_scrollTo;
        int m_rowIndex = 0;
        ObjectIcons m_icons;
    };
}
