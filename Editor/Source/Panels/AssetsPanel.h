#pragma once

#include "Core/Guid.h"

#include <filesystem>
#include <string>
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
        void DrawFolderTree(const std::string& folder, const std::string& label);
        void DrawCreateMenu();
        void DrawItemContextMenu(const Core::Guid& guid);
        void OpenAsset(const Core::Guid& guid);
        std::string CurrentFolder() const;

        EditorApplication& m_application;
        std::string m_filter;
        float m_tileSize = 84.0f;
        Core::Guid m_renaming;
        std::string m_renameBuffer;
        bool m_focusRename = false;
        Core::Guid m_pendingDelete;
    };
}
