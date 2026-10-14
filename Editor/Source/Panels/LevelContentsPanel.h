#pragma once

#include "Panels/LevelContents/LevelContentsTree.h"

#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class LevelContentsPanel
    {
    public:
        explicit LevelContentsPanel(EditorApplication& application);
        void Draw();
        void BeginRenameSelection();

    private:
        void DrawToolbar();
        void HandleTreeInput();

        EditorApplication& m_application;
        LevelContentsTree m_tree;
        std::string m_filter;
    };
}
