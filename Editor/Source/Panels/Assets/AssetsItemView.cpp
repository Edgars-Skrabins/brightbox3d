#include "Panels/Assets/AssetsItemView.h"

#include "Assets/AssetDatabase.h"
#include "Core/StringUtils.h"
#include "DragDrop.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "ImGui/ImGuiRenderer.h"
#include "Panels/Assets/AssetActions.h"
#include "Panels/Assets/AssetsContextMenu.h"
#include "Panels/Assets/AssetTypeStyle.h"
#include "Panels/Assets/AssetsFolderTree.h"
#include "Theme.h"
#include "Thumbnails.h"

#include <misc/cpp/imgui_stdlib.h>

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
        constexpr float FilterWidth = 160.0f;
        constexpr float TileSliderWidth = 80.0f;
        constexpr float MinTileSize = 56.0f;
        constexpr float MaxTileSize = 160.0f;
        constexpr float MinDropZoneHeight = 40.0f;
        constexpr float TileIconScale = 0.55f;
        constexpr Assets::AssetType BuiltinTypes[] = { Assets::AssetType::Model, Assets::AssetType::Material };

        bool SortFoldersFirst(const Assets::AssetRecord& a, const Assets::AssetRecord& b)
        {
            bool aFolder = a.type == Assets::AssetType::Folder;
            bool bFolder = b.type == Assets::AssetType::Folder;
            if (aFolder != bFolder)
            {
                return aFolder;
            }
            return a.name < b.name;
        }
    }

    AssetsItemView::AssetsItemView(EditorApplication& application, AssetsPanelState& state) : m_application(application), m_state(state)
    {
    }

    std::string AssetsItemView::CurrentFolder() const
    {
        return m_application.GetContext().GetAssetCreationService().GetAssetsFolder();
    }

    void AssetsItemView::DrawToolbar()
    {
        if (ImGui::Button(Text(EditorText::CreateAssetButton)))
        {
            ImGui::OpenPopup("CreateAsset");
        }
        if (ImGui::BeginPopup("CreateAsset"))
        {
            AssetsContextMenu::DrawCreateMenu(m_application, CurrentFolder());
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (!CurrentFolder().empty() && ImGui::Button(Text(EditorText::FolderUp)))
        {
            m_application.GetContext().GetAssetCreationService().SetAssetsFolder(CurrentFolder() == BuiltinFolder ? "" : ParentFolder(CurrentFolder()));
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", Text(EditorText::AssetsFolderPath, CurrentFolder()).c_str());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(FilterWidth);
        ImGui::InputTextWithHint("##filter", Text(EditorText::SearchHint), &m_filter);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(TileSliderWidth);
        ImGui::SliderFloat("##size", &m_tileSize, MinTileSize, MaxTileSize, "");
    }

    std::vector<Assets::AssetRecord> AssetsItemView::CollectItems() const
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        std::string filter = Core::ToLower(m_filter);
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
        std::stable_sort(items.begin(), items.end(), SortFoldersFirst);
        return items;
    }

    void AssetsItemView::HandleItemDrop(const Assets::AssetRecord& record)
    {
        if (record.type != Assets::AssetType::Folder || !ImGui::BeginDragDropTarget())
        {
            return;
        }
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        if (std::optional<Core::Guid> dragged = AcceptPayload(DragDropPayload::Asset))
        {
            if (const Assets::AssetRecord* draggedRecord = database.FindRecord(*dragged); draggedRecord && *dragged != record.guid)
            {
                database.MoveAsset(*dragged, record.path + "/" + draggedRecord->name);
            }
        }
        ImGui::EndDragDropTarget();
    }

    void AssetsItemView::DrawThumbnail(const Assets::AssetRecord& record, const ImVec2& cursor)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 imageMin(cursor.x + ThumbnailInset, cursor.y + ThumbnailInset);
        ImVec2 imageMax(cursor.x + m_tileSize - ThumbnailInset, cursor.y + m_tileSize - ThumbnailInset);
        const AssetTypeStyle& style = GetAssetTypeStyle(record.type);
        ImU32 color = EditorColorU32(style.color);
        float iconSize = m_tileSize * TileIconScale;
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

    void AssetsItemView::DrawLabel(const Assets::AssetRecord& record, const ImVec2& cursor)
    {
        if (!m_state.rename.IsRenaming(record.guid))
        {
            ImVec2 textSize = ImGui::CalcTextSize(record.name.c_str(), nullptr, false, m_tileSize);
            ImVec4 clip(cursor.x, cursor.y + m_tileSize, cursor.x + m_tileSize, cursor.y + m_tileSize + ImGui::GetTextLineHeightWithSpacing() * LabelLines);
            ImGui::GetWindowDrawList()->AddText(nullptr, 0.0f, ImVec2(cursor.x + std::max(0.0f, (m_tileSize - textSize.x) * 0.5f), cursor.y + m_tileSize), ImGui::GetColorU32(ImGuiCol_Text), record.name.c_str(), nullptr, m_tileSize, &clip);
            return;
        }
        ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + m_tileSize));
        if (std::optional<std::string> renamed = m_state.rename.Draw(m_tileSize))
        {
            AssetActions::CommitRename(record, *renamed);
        }
    }

    void AssetsItemView::DrawItem(const Assets::AssetRecord& record)
    {
        SelectionService& selection = m_application.GetContext().GetSelectionService();
        ImGui::PushID(record.guid.ToString().c_str());
        ImGui::BeginGroup();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        bool selected = selection.GetFocus() == SelectionFocus::Asset && selection.GetSelectedAsset() == record.guid;
        if (ImGui::Selectable("##tile", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(m_tileSize, m_tileSize + ImGui::GetTextLineHeightWithSpacing() * LabelLines)))
        {
            selection.SelectAsset(record.guid);
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                AssetActions::Open(m_application, record.guid);
            }
        }
        if (ImGui::BeginDragDropSource())
        {
            SetPayload(record.guid, DragDropPayload::Asset);
            ImGui::TextUnformatted(record.name.c_str());
            ImGui::EndDragDropSource();
        }
        HandleItemDrop(record);
        if (ImGui::BeginPopupContextItem("ItemContext"))
        {
            selection.SelectAsset(record.guid);
            AssetsContextMenu::Draw(m_application, m_state, TargetForAsset(record));
            ImGui::EndPopup();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        {
            ImGui::SetTooltip("%s\n%s", record.path.c_str(), Assets::AssetTypeName(record.type));
        }
        DrawThumbnail(record, cursor);
        DrawLabel(record, cursor);
        ImGui::EndGroup();
        ImGui::PopID();
    }

    void AssetsItemView::DrawDropZone()
    {
        ImGui::SetCursorPos(ImVec2(0, std::max(ImGui::GetCursorPosY(), 0.0f)));
        ImGui::InvisibleButton("##dropzone", ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x), std::max(MinDropZoneHeight, ImGui::GetContentRegionAvail().y)));
        if (ImGui::BeginPopupContextItem("BackgroundContext"))
        {
            AssetsContextMenu::Draw(m_application, m_state, { {}, CurrentFolder() });
            ImGui::EndPopup();
        }
        if (!ImGui::BeginDragDropTarget())
        {
            return;
        }
        EditorContext& context = m_application.GetContext();
        if (std::optional<Core::Guid> id = AcceptPayload(DragDropPayload::Object))
        {
            if (Level::Level* level = context.GetLevelService().GetActiveLevel(); level && !context.GetLevelService().IsPlaying())
            {
                if (Level::LevelObject* object = level->FindObject(*id))
                {
                    context.GetAssetCreationService().CreateVariantAsset(*object, CurrentFolder());
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    void AssetsItemView::Draw()
    {
        DrawToolbar();
        ImGui::BeginChild("Items", ImVec2(0, 0), ImGuiChildFlags_None);
        std::vector<Assets::AssetRecord> items = CollectItems();
        float cell = m_tileSize + TileSpacing;
        int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell));
        int column = 0;
        for (const Assets::AssetRecord& record : items)
        {
            DrawItem(record);
            if (++column < columns)
            {
                ImGui::SameLine(0.0f, TileSpacing);
            }
            else
            {
                column = 0;
            }
        }
        DrawDropZone();
        if (ImGui::BeginPopupContextWindow("GapContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            AssetsContextMenu::Draw(m_application, m_state, { {}, CurrentFolder() });
            ImGui::EndPopup();
        }
        ImGui::EndChild();
    }
}
