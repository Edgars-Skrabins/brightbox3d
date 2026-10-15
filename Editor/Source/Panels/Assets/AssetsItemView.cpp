#include "Panels/Assets/AssetsItemView.h"

#include "Assets/AssetDatabase.h"
#include "Core/StringUtils.h"
#include "DragDrop.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "ImGui/ImGuiRenderer.h"
#include "Panels/Assets/AssetActions.h"
#include "Panels/Assets/AssetsBottomBar.h"
#include "Panels/Assets/AssetsContextMenu.h"
#include "Panels/Assets/AssetsDropZone.h"
#include "Panels/Assets/AssetTypeStyle.h"
#include "Panels/Assets/AssetsFolderTree.h"
#include "Panels/Assets/AssetsToolbar.h"
#include "Theme.h"
#include "Thumbnails.h"

#include <algorithm>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr float TileSpacing = 16.0f;
        constexpr float ThumbnailInset = 4.0f;
        constexpr float ThumbnailRounding = 4.0f;
        constexpr float TypeStripeHeight = 3.0f;
        constexpr float LabelLines = 2.0f;
        constexpr float TileIconScale = 0.55f;
        constexpr float SelectionBarWidth = 2.0f;
        constexpr Assets::AssetType BuiltinTypes[] = { Assets::AssetType::Model, Assets::AssetType::Material };
    }

    AssetsItemView::AssetsItemView(EditorApplication& application, AssetsPanelState& state) : m_application(application), m_state(state)
    {
    }

    std::string AssetsItemView::CurrentFolder() const
    {
        return m_state.folder;
    }

    bool AssetsItemView::IsSelected(const Assets::AssetRecord& record) const
    {
        SelectionService& selection = m_application.GetContext().GetSelectionService();
        return selection.GetFocus() == SelectionFocus::Asset && selection.GetSelectedAsset() == record.guid;
    }

    void AssetsItemView::DrawBreadcrumb(const std::string& label, const std::string& folder)
    {
        ImGui::PushID(folder.c_str());
        if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_None, ImGui::CalcTextSize(label.c_str())))
        {
            m_state.folder = folder;
        }
        AssetActions::AcceptMoveDrop(folder);
        ImGui::PopID();
    }

    void AssetsItemView::DrawBreadcrumbs()
    {
        std::string folder = CurrentFolder();
        ImGui::AlignTextToFramePadding();
        if (folder == BuiltinFolder)
        {
            ImGui::TextUnformatted(Text(EditorText::BuiltinAssets));
            return;
        }
        DrawBreadcrumb(Text(EditorText::AssetsTitle), "");
        if (folder.empty())
        {
            return;
        }
        std::string path;
        for (const std::string& part : Core::Split(folder, '/'))
        {
            path += (path.empty() ? "" : "/") + part;
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::TextDisabled("%s", IconGlyph(Icon::ChevronRight));
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            DrawBreadcrumb(part, path);
        }
    }

    void AssetsItemView::DrawToolbar()
    {
        AssetsToolbar::DrawCreateButton(m_application, CurrentFolder());
        ImGui::SameLine();
        if (!CurrentFolder().empty() && ImGui::Button(Text(EditorText::FolderUp)))
        {
            m_state.folder = CurrentFolder() == BuiltinFolder ? "" : ParentFolder(CurrentFolder());
        }
        ImGui::SameLine();
        DrawBreadcrumbs();
        ImGui::SameLine();
        AssetsToolbar::DrawSearch(m_state);
        ImGui::SameLine();
        AssetsToolbar::DrawPanelMenu();
    }

    std::vector<Assets::AssetRecord> AssetsItemView::CollectItems() const
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        std::string filter = Core::ToLower(m_state.filter);
        std::string folder = CurrentFolder();
        std::vector<Assets::AssetRecord> items;
        for (const Assets::AssetRecord& record : database.GetRecords())
        {
            if (database.IsBuiltin(record.guid))
            {
                continue;
            }
            bool matches = filter.empty() ? ParentFolder(record.path) == folder : Core::ToLower(record.path).find(filter) != std::string::npos;
            if (matches)
            {
                items.push_back(record);
            }
        }
        if (folder == BuiltinFolder)
        {
            for (Assets::AssetType type : BuiltinTypes)
            {
                for (const Assets::AssetRecord& record : database.GetRecords(type))
                {
                    if (database.IsBuiltin(record.guid))
                    {
                        items.push_back(record);
                    }
                }
            }
        }
        std::stable_sort(items.begin(), items.end(), FoldersFirst);
        return items;
    }

    void AssetsItemView::HandleItemClick(const Assets::AssetRecord& record)
    {
        m_application.GetContext().GetSelectionService().SelectAsset(record.guid);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            AssetActions::Open(m_application, m_state, record.guid);
        }
    }

    void AssetsItemView::HandleItemInteraction(const Assets::AssetRecord& record)
    {
        if (ImGui::BeginDragDropSource())
        {
            SetPayload(record.guid, DragDropPayload::Asset);
            ImGui::TextUnformatted(record.name.c_str());
            ImGui::EndDragDropSource();
        }
        if (record.type == Assets::AssetType::Folder)
        {
            AssetActions::AcceptMoveDrop(record.path);
        }
        if (ImGui::BeginPopupContextItem("ItemContext"))
        {
            m_application.GetContext().GetSelectionService().SelectAsset(record.guid);
            AssetsContextMenu::Draw(m_application, m_state, TargetForAsset(record));
            ImGui::EndPopup();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        {
            ImGui::SetTooltip("%s\n%s", record.path.c_str(), Assets::AssetTypeName(record.type));
        }
    }

    void AssetsItemView::DrawThumbnail(const Assets::AssetRecord& record, const ImVec2& cursor, float tileSize)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 imageMin(cursor.x + ThumbnailInset, cursor.y + ThumbnailInset);
        ImVec2 imageMax(cursor.x + tileSize - ThumbnailInset, cursor.y + tileSize - ThumbnailInset);
        const AssetTypeStyle& style = GetAssetTypeStyle(record.type);
        ImU32 color = EditorColorU32(style.color);
        float iconSize = tileSize * TileIconScale;
        if (record.type == Assets::AssetType::Folder)
        {
            DrawCenteredIcon(style.icon, imageMin, imageMax, iconSize, color);
            return;
        }
        if (RHI::Texture* thumbnail = m_application.GetThumbnails().Get(record.guid))
        {
            drawList->AddImage(ImGuiRenderer::ToTextureId(thumbnail), imageMin, imageMax);
        }
        else
        {
            drawList->AddRectFilled(imageMin, imageMax, EditorColorU32(EditorColor::Control), ThumbnailRounding);
            DrawCenteredIcon(style.icon, imageMin, imageMax, iconSize, color);
        }
        drawList->AddRectFilled(ImVec2(imageMin.x, imageMax.y - TypeStripeHeight), ImVec2(imageMax.x, imageMax.y), color);
    }

    void AssetsItemView::DrawTileLabel(const Assets::AssetRecord& record, const ImVec2& cursor, float tileSize)
    {
        if (!m_state.rename.IsRenaming(record.guid))
        {
            ImVec2 textSize = ImGui::CalcTextSize(record.name.c_str(), nullptr, false, tileSize);
            ImVec4 clip(cursor.x, cursor.y + tileSize, cursor.x + tileSize, cursor.y + tileSize + ImGui::GetTextLineHeightWithSpacing() * LabelLines);
            ImGui::GetWindowDrawList()->AddText(nullptr, 0.0f, ImVec2(cursor.x + std::max(0.0f, (tileSize - textSize.x) * 0.5f), cursor.y + tileSize), ImGui::GetColorU32(ImGuiCol_Text), record.name.c_str(), nullptr, tileSize, &clip);
            return;
        }
        ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + tileSize));
        if (std::optional<std::string> renamed = m_state.rename.Draw(tileSize))
        {
            AssetActions::CommitRename(record, *renamed);
        }
    }

    void AssetsItemView::DrawTile(const Assets::AssetRecord& record, float tileSize)
    {
        ImGui::PushID(record.guid.ToString().c_str());
        ImGui::BeginGroup();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        if (ImGui::Selectable("##tile", IsSelected(record), ImGuiSelectableFlags_AllowDoubleClick, ImVec2(tileSize, tileSize + ImGui::GetTextLineHeightWithSpacing() * LabelLines)))
        {
            HandleItemClick(record);
        }
        HandleItemInteraction(record);
        DrawThumbnail(record, cursor, tileSize);
        DrawTileLabel(record, cursor, tileSize);
        ImGui::EndGroup();
        ImGui::PopID();
    }

    void AssetsItemView::DrawRow(const Assets::AssetRecord& record, int index)
    {
        ImGui::PushID(record.guid.ToString().c_str());
        ImVec2 rowPosition = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        float height = ImGui::GetFrameHeight();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (index % 2 == 1)
        {
            drawList->AddRectFilled(rowPosition, ImVec2(rowPosition.x + width, rowPosition.y + height), EditorColorU32(EditorColor::RowStripe));
        }
        bool selected = IsSelected(record);
        if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(width, height)))
        {
            HandleItemClick(record);
        }
        HandleItemInteraction(record);
        if (selected)
        {
            drawList->AddRectFilled(rowPosition, ImVec2(rowPosition.x + SelectionBarWidth, rowPosition.y + height), EditorColorU32(EditorColor::Accent));
        }
        ImVec2 labelPosition(rowPosition.x + ImGui::GetStyle().FramePadding.x, rowPosition.y + ImGui::GetStyle().FramePadding.y);
        if (m_state.rename.IsRenaming(record.guid))
        {
            ImGui::SetCursorScreenPos(ImVec2(labelPosition.x, rowPosition.y));
            if (std::optional<std::string> renamed = m_state.rename.Draw(-FLT_MIN))
            {
                AssetActions::CommitRename(record, *renamed);
            }
        }
        else
        {
            DrawAssetLabel(labelPosition, record.type, record.name, false);
            const char* typeName = Assets::AssetTypeName(record.type);
            float typeX = rowPosition.x + width - ImGui::GetStyle().FramePadding.x - ImGui::CalcTextSize(typeName).x;
            drawList->AddText(ImVec2(typeX, labelPosition.y), EditorColorU32(EditorColor::TextDim), typeName);
        }
        ImGui::PopID();
    }

    void AssetsItemView::DrawGrid(const std::vector<Assets::AssetRecord>& items)
    {
        float tileSize = static_cast<float>(EditorPreferences::Get().GetAssetIconSize());
        float cell = tileSize + TileSpacing;
        int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell));
        int column = 0;
        for (const Assets::AssetRecord& record : items)
        {
            DrawTile(record, tileSize);
            if (++column < columns)
            {
                ImGui::SameLine(0.0f, TileSpacing);
            }
            else
            {
                column = 0;
            }
        }
    }

    void AssetsItemView::DrawList(const std::vector<Assets::AssetRecord>& items)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
        int index = 0;
        for (const Assets::AssetRecord& record : items)
        {
            DrawRow(record, index++);
        }
        ImGui::PopStyleVar();
    }

    void AssetsItemView::Draw()
    {
        DrawToolbar();
        ImGui::BeginChild("Items", ImVec2(0, -AssetsBottomBar::Height()), ImGuiChildFlags_None);
        std::vector<Assets::AssetRecord> items = CollectItems();
        if (EditorPreferences::Get().IsAssetListMode())
        {
            DrawList(items);
        }
        else
        {
            DrawGrid(items);
        }
        AssetsDropZone::Draw(m_application, m_state, CurrentFolder());
        ImGui::EndChild();
        AssetsBottomBar::Draw(m_application, true);
    }
}
