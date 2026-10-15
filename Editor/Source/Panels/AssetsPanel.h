#pragma once

#include "Panels/Assets/AssetsItemView.h"
#include "Panels/Assets/AssetsPanelState.h"
#include "Panels/Assets/AssetsTreeView.h"
#include "Panels/EditorPanel.h"

#include <filesystem>
#include <vector>

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class AssetsPanel : public EditorPanel
    {
    public:
        AssetsPanel(EditorApplication& application, int instanceIndex);
        void Draw() override;
        void ImportFiles(const std::vector<std::filesystem::path>& files);

        AssetsPanelState& GetState() { return m_state; }
        AssetsTarget GetCommandTarget() const;

    private:
        void DrawDeleteConfirmation();
        void DrawTwoColumns();

        EditorApplication& m_application;
        AssetsPanelState m_state;
        AssetsItemView m_items;
        AssetsTreeView m_tree;
    };
}
