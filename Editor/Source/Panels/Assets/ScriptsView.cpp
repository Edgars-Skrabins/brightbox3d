#include "Panels/Assets/ScriptsView.h"

#include "Assets/AssetType.h"
#include "Commands/CommandWidgets.h"
#include "Core/StringUtils.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "Panels/Assets/AssetTypeStyle.h"
#include "Panels/Assets/ScriptActions.h"
#include "Scripts/ScriptFiles.h"
#include "Services/ScriptLibrary.h"
#include "Theme.h"

#include <imgui.h>

#include <algorithm>
#include <optional>
#include <vector>

namespace BrightBox3D::Editor::ScriptsView
{
    namespace
    {
        constexpr float TileSpacing = 16.0f;
        constexpr float ThumbnailInset = 4.0f;
        constexpr float ThumbnailRounding = 4.0f;
        constexpr float TypeStripeHeight = 3.0f;
        constexpr float LabelLines = 2.0f;
        constexpr float TileIconScale = 0.55f;
        constexpr const char* ItemContextId = "ScriptContext";
        constexpr float MinBackgroundHeight = 40.0f;

        struct Item
        {
            Core::Guid id;
            std::string name;
            std::string folder;
            std::optional<ScriptEntry> script;
        };

        ScriptLibrary& Library(EditorApplication& application)
        {
            return application.GetContext().GetScriptLibrary();
        }

        Icon ItemIcon(const Item& item)
        {
            return item.script ? ScriptActions::ScriptIcon(*item.script) : GetAssetTypeStyle(Assets::AssetType::Folder).icon;
        }

        EditorColor ItemColor(const Item& item)
        {
            return item.script ? EditorColor::AssetScript : GetAssetTypeStyle(Assets::AssetType::Folder).color;
        }

        AssetsTarget ItemTarget(const Item& item)
        {
            return item.script ? ScriptActions::TargetForScript(*item.script) : ScriptActions::TargetForFolder(item.folder);
        }

        bool IsSelected(const AssetsPanelState& state, const Item& item)
        {
            return item.script && state.selectedScript == item.id;
        }

        std::vector<Item> CollectItems(EditorApplication& application, const AssetsPanelState& state)
        {
            ScriptLibrary& library = Library(application);
            std::vector<Item> items;
            std::string filter = Core::ToLower(state.filter);
            if (filter.empty())
            {
                for (const std::string& folder : library.ChildFolders(state.scriptFolder))
                {
                    items.push_back({ ScriptLibrary::FolderId(folder), ScriptFiles::FolderName(folder), folder, std::nullopt });
                }
            }
            for (const ScriptEntry& script : library.GetScripts())
            {
                bool matches = filter.empty() ? script.folder == state.scriptFolder : Core::ToLower(script.className).find(filter) != std::string::npos;
                if (matches)
                {
                    items.push_back({ script.id, script.className, script.folder, script });
                }
            }
            return items;
        }

        void CommitRename(EditorApplication& application, AssetsPanelState& state, const Item& item, const std::string& name)
        {
            if (!item.script)
            {
                ScriptActions::CommitFolderRename(application, state, item.folder, name);
            }
        }

        void HandleClick(EditorApplication& application, AssetsPanelState& state, const Item& item)
        {
            if (item.script)
            {
                state.selectedScript = item.id;
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                ScriptActions::Open(application, state, ItemTarget(item));
            }
        }

        void HandleInteraction(EditorApplication& application, AssetsPanelState& state, const Item& item)
        {
            if (item.script)
            {
                ScriptActions::BeginDrag(*item.script);
            }
            else
            {
                ScriptActions::BeginFolderDrag(item.folder);
                ScriptActions::AcceptMoveDrop(application, state, item.folder);
            }
            if (ImGui::BeginPopupContextItem(ItemContextId))
            {
                if (item.script)
                {
                    state.selectedScript = item.id;
                }
                DrawContextMenu(application, state, ItemTarget(item));
                ImGui::EndPopup();
            }
            if (item.script && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            {
                ImGui::SetTooltip("%s", Text(EditorText::ScriptTooltip, ScriptActions::DisplayPath(ItemTarget(item)) + "/" + item.script->PrimaryFile().filename().string(), ScriptActions::ScriptKind(*item.script)).c_str());
            }
        }

        void DrawRenameField(EditorApplication& application, AssetsPanelState& state, const Item& item, float width)
        {
            if (std::optional<std::string> renamed = state.rename.Draw(width))
            {
                CommitRename(application, state, item, *renamed);
            }
        }

        void DrawTile(EditorApplication& application, AssetsPanelState& state, const Item& item, float tileSize)
        {
            ImGui::PushID(item.id.ToString().c_str());
            ImGui::BeginGroup();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##tile", IsSelected(state, item), ImGuiSelectableFlags_AllowDoubleClick, ImVec2(tileSize, tileSize + ImGui::GetTextLineHeightWithSpacing() * LabelLines)))
            {
                HandleClick(application, state, item);
            }
            HandleInteraction(application, state, item);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 imageMin(cursor.x + ThumbnailInset, cursor.y + ThumbnailInset);
            ImVec2 imageMax(cursor.x + tileSize - ThumbnailInset, cursor.y + tileSize - ThumbnailInset);
            ImU32 color = EditorColorU32(ItemColor(item));
            if (item.script)
            {
                drawList->AddRectFilled(imageMin, imageMax, EditorColorU32(EditorColor::Control), ThumbnailRounding);
                drawList->AddRectFilled(ImVec2(imageMin.x, imageMax.y - TypeStripeHeight), imageMax, color);
            }
            DrawCenteredIcon(ItemIcon(item), imageMin, imageMax, tileSize * TileIconScale, color);
            if (state.rename.IsRenaming(item.id))
            {
                ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + tileSize));
                DrawRenameField(application, state, item, tileSize);
            }
            else
            {
                ImVec2 textSize = ImGui::CalcTextSize(item.name.c_str(), nullptr, false, tileSize);
                ImVec4 clip(cursor.x, cursor.y + tileSize, cursor.x + tileSize, cursor.y + tileSize + ImGui::GetTextLineHeightWithSpacing() * LabelLines);
                drawList->AddText(nullptr, 0.0f, ImVec2(cursor.x + std::max(0.0f, (tileSize - textSize.x) * 0.5f), cursor.y + tileSize), ImGui::GetColorU32(ImGuiCol_Text), item.name.c_str(), nullptr, tileSize, &clip);
            }
            ImGui::EndGroup();
            ImGui::PopID();
        }

        void DrawRow(EditorApplication& application, AssetsPanelState& state, const Item& item)
        {
            ImGui::PushID(item.id.ToString().c_str());
            ImVec2 rowPosition = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;
            if (ImGui::Selectable("##row", IsSelected(state, item), ImGuiSelectableFlags_AllowDoubleClick, ImVec2(width, ImGui::GetFrameHeight())))
            {
                HandleClick(application, state, item);
            }
            HandleInteraction(application, state, item);
            ImVec2 labelPosition(rowPosition.x + ImGui::GetStyle().FramePadding.x, rowPosition.y + ImGui::GetStyle().FramePadding.y);
            if (state.rename.IsRenaming(item.id))
            {
                ImGui::SetCursorScreenPos(ImVec2(labelPosition.x, rowPosition.y));
                DrawRenameField(application, state, item, -FLT_MIN);
            }
            else
            {
                DrawIconLabel(labelPosition, ItemIcon(item), ItemColor(item), item.name);
                if (item.script)
                {
                    std::string kind = ScriptActions::ScriptKind(*item.script);
                    float kindX = rowPosition.x + width - ImGui::GetStyle().FramePadding.x - ImGui::CalcTextSize(kind.c_str()).x;
                    ImGui::GetWindowDrawList()->AddText(ImVec2(kindX, labelPosition.y), EditorColorU32(EditorColor::TextDim), kind.c_str());
                }
            }
            ImGui::PopID();
        }

        bool DrawTreeNode(AssetsPanelState& state, const Item& item, const std::string& id, ImGuiTreeNodeFlags flags, FolderState folderState)
        {
            ImVec2 rowPosition = ImGui::GetCursorScreenPos();
            ImVec2 labelPosition(rowPosition.x + ImGui::GetTreeNodeToLabelSpacing(), rowPosition.y + ImGui::GetStyle().FramePadding.y);
            bool open = ImGui::TreeNodeEx(id.c_str(), flags, "%s", "");
            if (state.rename.IsRenaming(item.id))
            {
                return open;
            }
            Icon icon = item.script ? ItemIcon(item) : AssetTypeIcon(Assets::AssetType::Folder, open && !(flags & ImGuiTreeNodeFlags_Leaf) ? FolderState::Open : folderState);
            if (!item.script && item.folder.empty())
            {
                icon = Icon::FolderCode;
            }
            DrawIconLabel(labelPosition, icon, ItemColor(item), item.name);
            return open;
        }

        void DrawTreeRename(EditorApplication& application, AssetsPanelState& state, const Item& item)
        {
            if (!state.rename.IsRenaming(item.id))
            {
                return;
            }
            ImGui::SameLine();
            DrawRenameField(application, state, item, -FLT_MIN);
        }

        void DrawTreeFolder(EditorApplication& application, AssetsPanelState& state, const Item& folder, bool includeScripts)
        {
            ScriptLibrary& library = Library(application);
            std::vector<std::string> folders = library.ChildFolders(folder.folder);
            std::vector<ScriptEntry> scripts = includeScripts ? library.ScriptsIn(folder.folder) : std::vector<ScriptEntry>();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (folders.empty() && scripts.empty())
            {
                flags |= ImGuiTreeNodeFlags_Leaf;
            }
            if (state.root == AssetsRoot::Scripts && state.scriptFolder == folder.folder && !includeScripts)
            {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            ImGui::PushID(folder.id.ToString().c_str());
            bool open = DrawTreeNode(state, folder, "##folder", flags, FolderState::Closed);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                state.root = AssetsRoot::Scripts;
                state.scriptFolder = folder.folder;
            }
            HandleInteraction(application, state, folder);
            DrawTreeRename(application, state, folder);
            if (open)
            {
                for (const std::string& child : folders)
                {
                    DrawTreeFolder(application, state, { ScriptLibrary::FolderId(child), ScriptFiles::FolderName(child), child, std::nullopt }, includeScripts);
                }
                for (const ScriptEntry& script : scripts)
                {
                    Item item{ script.id, script.className, script.folder, script };
                    ImGui::PushID(item.id.ToString().c_str());
                    ImGuiTreeNodeFlags leaf = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                    if (IsSelected(state, item))
                    {
                        leaf |= ImGuiTreeNodeFlags_Selected;
                    }
                    DrawTreeNode(state, item, "##script", leaf, FolderState::Closed);
                    if (ImGui::IsItemClicked())
                    {
                        state.root = AssetsRoot::Scripts;
                        state.scriptFolder = item.folder;
                        HandleClick(application, state, item);
                    }
                    HandleInteraction(application, state, item);
                    DrawTreeRename(application, state, item);
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        Item RootItem()
        {
            return { ScriptLibrary::FolderId({}), Text(EditorText::ScriptsTitle), {}, std::nullopt };
        }
    }

    void DrawFolderTree(EditorApplication& application, AssetsPanelState& state)
    {
        DrawTreeFolder(application, state, RootItem(), false);
    }

    void DrawBreadcrumbs(AssetsPanelState& state)
    {
        ImGui::AlignTextToFramePadding();
        auto crumb = [&state](const std::string& label, const std::string& folder)
        {
            ImGui::PushID(folder.c_str());
            if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_None, ImGui::CalcTextSize(label.c_str())))
            {
                state.scriptFolder = folder;
            }
            ImGui::PopID();
        };
        crumb(Text(EditorText::ScriptsTitle), "");
        std::string path;
        for (const std::string& part : Core::Split(state.scriptFolder, '/'))
        {
            if (part.empty())
            {
                continue;
            }
            path = ScriptFiles::JoinFolder(path, part);
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::TextDisabled("%s", IconGlyph(Icon::ChevronRight));
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            crumb(part, path);
        }
    }

    void DrawItems(EditorApplication& application, AssetsPanelState& state)
    {
        std::vector<Item> items = CollectItems(application, state);
        if (EditorPreferences::Get().IsAssetListMode())
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
            for (const Item& item : items)
            {
                DrawRow(application, state, item);
            }
            ImGui::PopStyleVar();
        }
        else
        {
            float tileSize = static_cast<float>(EditorPreferences::Get().GetAssetIconSize());
            int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (tileSize + TileSpacing)));
            int column = 0;
            for (const Item& item : items)
            {
                DrawTile(application, state, item, tileSize);
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
        ImGui::SetCursorPos(ImVec2(0, std::max(ImGui::GetCursorPosY(), 0.0f)));
        ImGui::InvisibleButton("##scriptsbackground", ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x), std::max(MinBackgroundHeight, ImGui::GetContentRegionAvail().y)));
        ScriptActions::AcceptMoveDrop(application, state, state.scriptFolder);
        if (ImGui::BeginPopupContextItem("ScriptsBackground"))
        {
            DrawContextMenu(application, state, ScriptActions::TargetForFolder(state.scriptFolder));
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopupContextWindow("ScriptsGap", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawContextMenu(application, state, ScriptActions::TargetForFolder(state.scriptFolder));
            ImGui::EndPopup();
        }
    }

    void DrawTree(EditorApplication& application, AssetsPanelState& state)
    {
        DrawTreeFolder(application, state, RootItem(), true);
    }

    void DrawSearchResults(EditorApplication& application, AssetsPanelState& state)
    {
        std::string filter = Core::ToLower(state.filter);
        std::vector<ScriptEntry> scripts = Library(application).GetScripts();
        for (const ScriptEntry& script : scripts)
        {
            if (Core::ToLower(script.className).find(filter) != std::string::npos)
            {
                DrawRow(application, state, { script.id, script.className, script.folder, script });
            }
        }
    }

    void DrawCreateMenu(EditorApplication& application, AssetsPanelState& state, const std::string& folder)
    {
        if (ImGui::MenuItem(Assets::AssetTypeName(Assets::AssetType::Folder)))
        {
            ScriptActions::CreateFolder(application, state, folder);
        }
    }

    void DrawContextMenu(EditorApplication& application, AssetsPanelState& state, const AssetsTarget& target)
    {
        state.menuTarget = target;
        if (ImGui::BeginMenu(Text(EditorText::Create)))
        {
            DrawCreateMenu(application, state, target.folder);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        CommandMenuItem(CommandId::AssetShowInFileBrowser);
        CommandMenuItem(CommandId::AssetOpen);
        CommandMenuItem(CommandId::ScriptOpenHeader);
        CommandMenuItem(CommandId::AssetRename);
        CommandMenuItem(CommandId::AssetDelete);
        CommandMenuItem(CommandId::AssetCopyPath);
    }
}
