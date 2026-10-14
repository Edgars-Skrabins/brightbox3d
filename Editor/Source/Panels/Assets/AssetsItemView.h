#pragma once

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

    class AssetsItemView
    {
    public:
        AssetsItemView(EditorApplication& application, AssetsPanelState& state);

        void Draw();

    private:
        void DrawToolbar();
        void DrawBreadcrumbs();
        void DrawBreadcrumb(const std::string& label, const std::string& folder);
        std::vector<Assets::AssetRecord> CollectItems() const;
        void DrawGrid(const std::vector<Assets::AssetRecord>& items);
        void DrawList(const std::vector<Assets::AssetRecord>& items);
        void DrawTile(const Assets::AssetRecord& record, float tileSize);
        void DrawRow(const Assets::AssetRecord& record, int index);
        void HandleItemClick(const Assets::AssetRecord& record);
        void HandleItemInteraction(const Assets::AssetRecord& record);
        void DrawThumbnail(const Assets::AssetRecord& record, const ImVec2& cursor, float tileSize);
        void DrawTileLabel(const Assets::AssetRecord& record, const ImVec2& cursor, float tileSize);
        bool IsSelected(const Assets::AssetRecord& record) const;
        std::string CurrentFolder() const;

        EditorApplication& m_application;
        AssetsPanelState& m_state;
    };
}
