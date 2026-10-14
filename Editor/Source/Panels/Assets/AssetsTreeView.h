#pragma once

#include "Assets/AssetType.h"
#include "Panels/Assets/AssetsPanelState.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace BrightBox3D::Assets
{
    struct AssetRecord;
}

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class AssetsTreeView
    {
    public:
        AssetsTreeView(EditorApplication& application, AssetsPanelState& state);

        void Draw();

    private:
        void DrawToolbar();
        void DrawFolder(const std::string& folder, const std::string& label, const Assets::AssetRecord* record);
        void DrawAsset(const Assets::AssetRecord& record);
        void DrawBuiltin();
        void DrawSearchResults();
        bool DrawNode(const Assets::AssetRecord* record, const std::string& id, const std::string& label, Assets::AssetType type, ImGuiTreeNodeFlags flags);
        void HandleNodeInput(const Assets::AssetRecord* record, const std::string& folder);
        std::vector<Assets::AssetRecord> Children(const std::string& folder) const;
        bool IsSelected(const Core::Guid& guid) const;
        std::string CurrentFolder() const;

        EditorApplication& m_application;
        AssetsPanelState& m_state;
    };
}
