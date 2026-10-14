#include "Panels/Assets/AssetsItemView.h"

#include "Assets/AssetDatabase.h"
#include "Core/StringUtils.h"
#include "DragDrop.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "ImGui/ImGuiRenderer.h"
#include "Labels.h"
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
        constexpr float SizeSliderWidth = 120.0f;
        constexpr float MinDropZoneHeight = 40.0f;
        constexpr float TileIconScale = 0.55f;
        constexpr float SelectionBarWidth = 2.0f;
        constexpr int ListSliderGap = 16;
        constexpr int ListSliderStop = EditorPreferences::MinAssetIconSize - ListSliderGap;
        constexpr int ListSnapValue = ListSliderStop + ListSliderGap / 2;
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
            m_application.GetContext().GetAssetCreationService().SetAssetsFolder(folder);
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
        DrawBreadcrumbs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::max(FilterWidth, ImGui::GetContentRegionAvail().x));
        ImGui::InputTextWithHint("##filter", IconLabel(Icon::Search, EditorText::SearchHint).c_str(), &m_filter);
    }

    void AssetsItemView::DrawSizeSlider()
    {
        EditorPreferences& preferences = EditorPreferences::Get();
        int value = preferences.IsAssetListMode() ? ListSliderStop : preferences.GetAssetIconSize();
        ImGui::SetNextItemWidth(SizeSliderWidth);
        if (ImGui::SliderInt("##iconSize", &value, ListSliderStop, EditorPreferences::MaxAssetIconSize, ""))
        {
            preferences.SetAssetIconSize(value < ListSnapValue ? EditorPreferences::AssetListIconSize : value);
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            preferences.Save();
        }
    }

    void AssetsItemView::DrawBottomBar(bool showSizeSlider)
    {
        ImGui::Separator();
        SelectionService& selection = m_application.GetContext().GetSelectionService();
        const Assets::AssetRecord* record = selection.GetFocus() == SelectionFocus::Asset ? Assets::AssetDatabase::Get().FindRecord(selection.GetSelectedAsset()) : nullptr;
        float rowEnd = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", record ? AssetActions::DisplayPath(*record).c_str() : "");
        if (!showSizeSlider)
        {
            return;
        }
        ImGui::SameLine();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(std::max(cursor.x, rowEnd - SizeSliderWidth), cursor.y));
        DrawSizeSlider();
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

    void AssetsItemView::HandleItemClick(const Assets::AssetRecord& record)
    {
        m_application.GetContext().GetSelectionService().SelectAsset(record.guid);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            AssetActions::Open(m_application, record.guid);
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
        float bottomBarHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.0f + ImGui::GetStyle().CellPadding.y;
        ImGui::BeginChild("Items", ImVec2(0, -bottomBarHeight), ImGuiChildFlags_None);
        std::vector<Assets::AssetRecord> items = CollectItems();
        if (EditorPreferences::Get().IsAssetListMode())
        {
            DrawList(items);
        }
        else
        {
            DrawGrid(items);
        }
        DrawDropZone();
        if (ImGui::BeginPopupContextWindow("GapContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            AssetsContextMenu::Draw(m_application, m_state, { {}, CurrentFolder() });
            ImGui::EndPopup();
        }
        ImGui::EndChild();
        DrawBottomBar(true);
    }
}
