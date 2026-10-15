#pragma once

#include "Panels/EditorPanel.h"
#include "Panels/LevelContents/LevelContentsTree.h"

#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class LevelContentsPanel : public EditorPanel
    {
    public:
        LevelContentsPanel(EditorApplication& application, int instanceIndex);
        void Draw() override;
        void BeginRenameSelection();

    private:
        void DrawToolbar();
        void HandleTreeInput();

        EditorApplication& m_application;
        LevelContentsTree m_tree;
        std::string m_filter;
    };
}
