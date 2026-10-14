#pragma once

#include "Core/Guid.h"
#include "Widgets/InlineRename.h"

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
        explicit AssetsItemView(EditorApplication& application);

        void Draw(Core::Guid& pendingDelete);

    private:
        void DrawToolbar();
        std::vector<Assets::AssetRecord> CollectItems() const;
        void DrawItem(const Assets::AssetRecord& record, Core::Guid& pendingDelete);
        void DrawThumbnail(const Assets::AssetRecord& record, const ImVec2& cursor);
        void DrawLabel(const Assets::AssetRecord& record, const ImVec2& cursor);
        void HandleItemDrop(const Assets::AssetRecord& record);
        void DrawDropZone();
        std::string CurrentFolder() const;

        EditorApplication& m_application;
        std::string m_filter;
        float m_tileSize = 84.0f;
        InlineRename m_rename;
    };
}
