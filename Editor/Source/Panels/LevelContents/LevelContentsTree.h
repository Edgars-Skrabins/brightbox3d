#pragma once

#include "Core/Guid.h"
#include "ObjectIcons.h"
#include "Widgets/InlineRename.h"

#include <imgui.h>

#include <string>

namespace BrightBox3D::Level
{
    class Level;
    class LevelCollection;
    class LevelObject;
}

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class LevelContentsTree
    {
    public:
        explicit LevelContentsTree(EditorApplication& application);

        void DrawLevelRow(Level::Level& level);
        void DrawCollections(Level::Level& level, const std::string& filter);
        void BeginRename(const Level::LevelObject& object);

    private:
        struct RowStyle
        {
            const char* icon = nullptr;
            ImVec4 iconColor;
            ImVec4 nameColor;
        };

        void DrawCollection(Level::LevelCollection& collection);
        void DrawObject(Level::LevelObject& object);
        void DrawRowBackground(bool selected);
        void TrackScrollTarget();
        void OpenTowardScrollTarget(Level::LevelObject& object);
        RowStyle StyleFor(Level::LevelObject& object);
        std::string SuffixFor(const Level::LevelObject& object) const;
        bool DrawObjectRow(Level::LevelObject& object, ImGuiTreeNodeFlags flags);
        void HandleObjectInput(Level::LevelObject& object);
        void HandleObjectDrop(Level::LevelObject& object);
        void HandleCollectionDrop(Level::LevelCollection& collection);
        bool MatchesFilter(const Level::LevelObject& object) const;

        EditorApplication& m_application;
        InlineRename m_rename;
        ObjectIcons m_icons;
        std::string m_filter;
        Core::Guid m_scrollTo;
        Core::Guid m_lastSelected;
        int m_rowIndex = 0;
    };
}
