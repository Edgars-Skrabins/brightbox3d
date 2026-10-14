#pragma once

#include "Core/Guid.h"
#include "Panels/Assets/AssetsItemView.h"

#include <filesystem>
#include <vector>

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class AssetsPanel
    {
    public:
        explicit AssetsPanel(EditorApplication& application);
        void Draw();
        void ImportFiles(const std::vector<std::filesystem::path>& files);

    private:
        void DrawDeleteConfirmation();

        EditorApplication& m_application;
        AssetsItemView m_items;
        Core::Guid m_pendingDelete;
    };
}
