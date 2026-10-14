#include "Panels/AssetsPanel.h"

#include "Assets/AssetDatabase.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "EditorWindows.h"
#include "ImGui/ImGuiRenderer.h"
#include "Labels.h"
#include "Level/LevelSerializer.h"
#include "Panels/LevelViewPanel.h"
#include "Rendering/MaterialAsset.h"
#include "Serialization/ObjectSerializer.h"
#include "Theme.h"
#include "Thumbnails.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <set>

namespace BrightBox3D::Editor
{
    namespace
    {
        std::string Parent(const std::string& path)
        {
            return std::filesystem::path(path).parent_path().generic_string();
        }

        ImU32 TypeColor(Assets::AssetType type)
        {
            switch (type)
            {
            case Assets::AssetType::Folder:
                return EditorColorU32(EditorColor::AssetFolder);
            case Assets::AssetType::Level:
                return EditorColorU32(EditorColor::AssetLevel);
            case Assets::AssetType::LevelCollection:
                return EditorColorU32(EditorColor::AssetCollection);
            case Assets::AssetType::LevelObjectVariant:
                return EditorColorU32(EditorColor::AssetLevelObject);
            case Assets::AssetType::LevelObjectTemplate:
                return EditorColorU32(EditorColor::AssetTemplate);
            case Assets::AssetType::Material:
                return EditorColorU32(EditorColor::AssetMaterial);
            case Assets::AssetType::Model:
                return EditorColorU32(EditorColor::AssetModel);
            case Assets::AssetType::Texture:
                return EditorColorU32(EditorColor::AssetTexture);
            case Assets::AssetType::Screen:
            case Assets::AssetType::Widget:
                return EditorColorU32(EditorColor::AssetUI);
            case Assets::AssetType::StyleSheet:
                return EditorColorU32(EditorColor::AssetStyleSheet);
            default:
                return EditorColorU32(EditorColor::AssetOther);
            }
        }

        const char* TypeGlyph(Assets::AssetType type)
        {
            switch (type)
            {
            case Assets::AssetType::Folder:
                return Text(EditorText::AssetBadgeFolder);
            case Assets::AssetType::Level:
                return Text(EditorText::AssetBadgeLevel);
            case Assets::AssetType::LevelCollection:
                return Text(EditorText::AssetBadgeLevelCollection);
            case Assets::AssetType::LevelObjectVariant:
                return Text(EditorText::AssetBadgeLevelObjectVariant);
            case Assets::AssetType::LevelObjectTemplate:
                return Text(EditorText::AssetBadgeLevelObjectTemplate);
            case Assets::AssetType::Material:
                return Text(EditorText::AssetBadgeMaterial);
            case Assets::AssetType::Model:
                return Text(EditorText::AssetBadgeModel);
            case Assets::AssetType::Texture:
                return Text(EditorText::AssetBadgeTexture);
            case Assets::AssetType::Screen:
                return Text(EditorText::AssetBadgeScreen);
            case Assets::AssetType::Widget:
                return Text(EditorText::AssetBadgeWidget);
            case Assets::AssetType::StyleSheet:
                return Text(EditorText::AssetBadgeStyleSheet);
            case Assets::AssetType::Font:
                return Text(EditorText::AssetBadgeFont);
            default:
                return Text(EditorText::AssetBadgeUnknown);
            }
        }
    }

    AssetsPanel::AssetsPanel(EditorApplication& application) : m_application(application)
    {
    }

    std::string AssetsPanel::CurrentFolder() const
    {
        return m_application.GetContext().GetAssetCreationService().GetAssetsFolder();
    }

    void AssetsPanel::ImportFiles(const std::vector<std::filesystem::path>& files)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        if (!database.IsOpen())
        {
            return;
        }
        std::filesystem::path target = database.GetAssetsDirectory() / CurrentFolder();
        for (const std::filesystem::path& file : files)
        {
            std::error_code error;
            std::filesystem::path destination = target / file.filename();
            if (std::filesystem::exists(destination))
            {
                destination = database.GetAssetsDirectory() / database.MakeUniquePath((CurrentFolder().empty() ? "" : CurrentFolder() + "/") + file.filename().string());
            }
            if (std::filesystem::is_directory(file))
            {
                std::filesystem::copy(file, destination, std::filesystem::copy_options::recursive, error);
            }
            else
            {
                std::filesystem::copy_file(file, destination, error);
            }
            if (error)
            {
                Core::Log::Error(Text(EditorText::ImportFailed, file.generic_string(), error.message()));
            }
            else
            {
                Core::Log::Info(Text(EditorText::Imported, destination.filename().string()));
            }
        }
        database.Refresh();
    }

    void AssetsPanel::OpenAsset(const Core::Guid& guid)
    {
        EditorContext& context = m_application.GetContext();
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        const Assets::AssetRecord* record = database.FindRecord(guid);
        if (!record)
        {
            return;
        }
        switch (record->type)
        {
        case Assets::AssetType::Folder:
            context.GetAssetCreationService().SetAssetsFolder(record->path);
            break;
        case Assets::AssetType::Level:
            context.GetLevelService().OpenLevel(guid);
            break;
        case Assets::AssetType::LevelObjectVariant:
        case Assets::AssetType::LevelObjectTemplate:
            context.GetLevelObjectEditService().OpenLevelObjectEditor(guid);
            break;
        case Assets::AssetType::Screen:
        case Assets::AssetType::Widget:
        case Assets::AssetType::StyleSheet:
            m_application.OpenFileInEditor(database.GetAbsolutePath(*record).string(), 0);
            break;
        default:
            context.GetSelectionService().SelectAsset(guid);
            break;
        }
    }

    void AssetsPanel::DrawCreateMenu()
    {
        EditorContext& context = m_application.GetContext();
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        std::string prefix = CurrentFolder().empty() ? "" : CurrentFolder() + "/";
        if (ImGui::MenuItem(Text(EditorText::AssetTypeFolder)))
        {
            std::string path = database.MakeUniquePath(prefix + Text(EditorText::NewFolderName) + Assets::AssetTypeExtension(Assets::AssetType::Folder));
            Core::FileSystem::EnsureDirectory(database.GetAssetsDirectory() / path);
            database.Refresh();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(Text(EditorText::AssetTypeLevel)))
        {
            Core::Guid guid = Level::LevelSerializer::CreateLevelAsset(prefix + Text(EditorText::NewLevelName) + Assets::AssetTypeExtension(Assets::AssetType::Level));
            context.GetSelectionService().SelectAsset(guid);
        }
        if (ImGui::MenuItem(Text(EditorText::AssetTypeLevelCollection)))
        {
            Serialization::JsonDocument document;
            Level::LevelSerializer::WriteEmptyCollection(Text(EditorText::NewCollectionName), document, document.GetAllocator());
            context.GetSelectionService().SelectAsset(database.CreateJsonAsset(database.MakeUniquePath(prefix + Text(EditorText::NewCollectionName) + Assets::AssetTypeExtension(Assets::AssetType::LevelCollection)), document));
        }
        if (ImGui::MenuItem(Text(EditorText::AssetTypeLevelObjectTemplate)))
        {
            context.GetSelectionService().SelectAsset(context.GetAssetCreationService().CreateTemplateAsset(nullptr, CurrentFolder()));
        }
        if (ImGui::MenuItem(Text(EditorText::AssetTypeMaterial)))
        {
            Rendering::MaterialAsset material;
            Serialization::JsonDocument document;
            Serialization::SerializeProperties(material, document, document.GetAllocator());
            context.GetSelectionService().SelectAsset(database.CreateJsonAsset(database.MakeUniquePath(prefix + Text(EditorText::NewMaterialName) + Assets::AssetTypeExtension(Assets::AssetType::Material)), document));
        }
        ImGui::Separator();
        if (ImGui::MenuItem(Text(EditorText::AssetTypeScreen)))
        {
            std::string path = database.MakeUniquePath(prefix + Text(EditorText::NewScreenName) + Assets::AssetTypeExtension(Assets::AssetType::Screen));
            Core::FileSystem::WriteText(database.GetAssetsDirectory() / path, "<Screen>\n    <Panel class=\"root\">\n        <Label text=\"Hello BrightBox3D\" />\n    </Panel>\n</Screen>\n");
            database.Refresh();
        }
        if (ImGui::MenuItem(Text(EditorText::AssetTypeWidget)))
        {
            std::string path = database.MakeUniquePath(prefix + Text(EditorText::NewWidgetName) + Assets::AssetTypeExtension(Assets::AssetType::Widget));
            Core::FileSystem::WriteText(database.GetAssetsDirectory() / path, "<Widget>\n    <Panel class=\"widget\">\n        <Label text=\"Widget\" />\n    </Panel>\n</Widget>\n");
            database.Refresh();
        }
        if (ImGui::MenuItem(Text(EditorText::AssetTypeStyleSheet)))
        {
            std::string path = database.MakeUniquePath(prefix + Text(EditorText::NewStyleSheetName) + Assets::AssetTypeExtension(Assets::AssetType::StyleSheet));
            Core::FileSystem::WriteText(database.GetAssetsDirectory() / path, ".root {\n    width: 100%;\n    height: 100%;\n    padding: 16px;\n}\n");
            database.Refresh();
        }
    }

    void AssetsPanel::DrawItemContextMenu(const Core::Guid& guid)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        const Assets::AssetRecord* record = database.FindRecord(guid);
        if (!record)
        {
            return;
        }
        bool builtin = database.IsBuiltin(guid);
        if (ImGui::MenuItem(Text(EditorText::Open)))
        {
            OpenAsset(guid);
        }
        ImGui::BeginDisabled(builtin);
        if (ImGui::MenuItem(Text(EditorText::Rename)))
        {
            m_renaming = guid;
            std::string name = record->name;
            m_renameBuffer = record->type == Assets::AssetType::Folder ? name : name.substr(0, name.find('.'));
            m_focusRename = true;
        }
        if (ImGui::MenuItem(Text(EditorText::Delete)))
        {
            m_pendingDelete = guid;
        }
        ImGui::EndDisabled();
        if (record->type == Assets::AssetType::Level && ImGui::MenuItem(Text(EditorText::SetStartupLevel)))
        {
            m_application.GetContext().GetProjectService().GetProject().SetStartupLevel(guid);
            m_application.GetContext().GetProjectService().GetProject().Save();
        }
        if (record->type == Assets::AssetType::LevelObjectTemplate && ImGui::MenuItem(Text(EditorText::CreateVariant)))
        {
            m_application.GetContext().GetAssetCreationService().CreateVariantFromTemplate(guid);
        }
        ImGui::Separator();
        if (!builtin && ImGui::MenuItem(Text(EditorText::RevealInFileBrowser)))
        {
            m_application.RevealInFileBrowser(database.GetAbsolutePath(*record));
        }
        if (!builtin && record->type != Assets::AssetType::Folder && ImGui::MenuItem(Text(EditorText::Reimport)))
        {
            std::string error;
            database.Unload(guid);
            if (database.CookAsset(*record, nullptr, error))
            {
                database.OnAssetChanged.Broadcast(guid);
            }
            else
            {
                Core::Log::Error(Text(EditorText::ReimportFailed, error));
            }
        }
    }

    void AssetsPanel::DrawFolderTree(const std::string& folder, const std::string& label)
    {
        EditorContext& context = m_application.GetContext();
        std::vector<Assets::AssetRecord> folders;
        for (const Assets::AssetRecord& record : Assets::AssetDatabase::Get().GetRecords(Assets::AssetType::Folder))
        {
            if (Parent(record.path) == folder)
            {
                folders.push_back(record);
            }
        }
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (folders.empty())
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (folder.empty())
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if (context.GetAssetCreationService().GetAssetsFolder() == folder)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        bool open = ImGui::TreeNodeEx(folder.empty() ? "##root" : folder.c_str(), flags, "%s", label.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            context.GetAssetCreationService().SetAssetsFolder(folder);
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRIGHT_ASSET"))
            {
                Core::Guid dragged = *static_cast<const Core::Guid*>(payload->Data);
                if (const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(dragged))
                {
                    Assets::AssetDatabase::Get().MoveAsset(dragged, (folder.empty() ? "" : folder + "/") + record->name);
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (open)
        {
            for (const Assets::AssetRecord& child : folders)
            {
                DrawFolderTree(child.path, child.name);
            }
            ImGui::TreePop();
        }
    }

    void AssetsPanel::Draw()
    {
        if (!ImGui::Begin(WindowTitle(EditorWindow::Assets).c_str()))
        {
            ImGui::End();
            return;
        }
        EditorContext& context = m_application.GetContext();
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        if (!database.IsOpen())
        {
            ImGui::TextDisabled("%s", Text(EditorText::NoProjectOpen));
            ImGui::End();
            return;
        }

        PushSelectionColors();
        if (ImGui::BeginTable("AssetsLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Items", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::BeginChild("FolderTree");
            DrawFolderTree("", Text(EditorText::AssetsTitle));
            bool showBuiltin = context.GetAssetCreationService().GetAssetsFolder() == "Builtin";
            if (ImGui::Selectable(Text(EditorText::BuiltinAssets), showBuiltin))
            {
                context.GetAssetCreationService().SetAssetsFolder("Builtin");
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);
            if (ImGui::Button("+ Create"))
            {
                ImGui::OpenPopup("CreateAsset");
            }
            if (ImGui::BeginPopup("CreateAsset"))
            {
                DrawCreateMenu();
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            if (!CurrentFolder().empty() && ImGui::Button(Text(EditorText::FolderUp)))
            {
                context.GetAssetCreationService().SetAssetsFolder(CurrentFolder() == "Builtin" ? "" : Parent(CurrentFolder()));
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", Text(EditorText::AssetsFolderPath, CurrentFolder()).c_str());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputTextWithHint("##filter", Text(EditorText::SearchHint), &m_filter);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::SliderFloat("##size", &m_tileSize, 56.0f, 160.0f, "");

            ImGui::BeginChild("Items", ImVec2(0, 0), ImGuiChildFlags_None);
            std::string lowerFilter = m_filter;
            std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
            std::vector<Assets::AssetRecord> items;
            for (const Assets::AssetRecord& record : database.GetRecords())
            {
                if (database.IsBuiltin(record.guid))
                {
                    continue;
                }
                bool inFolder = Parent(record.path) == CurrentFolder();
                if (!lowerFilter.empty())
                {
                    std::string lower = record.path;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (lower.find(lowerFilter) == std::string::npos)
                    {
                        continue;
                    }
                }
                else if (!inFolder)
                {
                    continue;
                }
                items.push_back(record);
            }
            if (CurrentFolder() == "Builtin")
            {
                for (Assets::AssetType type : { Assets::AssetType::Model, Assets::AssetType::Material })
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
            std::stable_sort(items.begin(), items.end(), [](const Assets::AssetRecord& a, const Assets::AssetRecord& b)
            {
                bool aFolder = a.type == Assets::AssetType::Folder;
                bool bFolder = b.type == Assets::AssetType::Folder;
                if (aFolder != bFolder)
                {
                    return aFolder;
                }
                return a.name < b.name;
            });

            float cell = m_tileSize + 16.0f;
            int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell));
            int column = 0;
            for (const Assets::AssetRecord& record : items)
            {
                ImGui::PushID(record.guid.ToString().c_str());
                ImGui::BeginGroup();
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                bool selected = context.GetSelectionService().GetFocus() == SelectionFocus::Asset && context.GetSelectionService().GetSelectedAsset() == record.guid;
                if (ImGui::Selectable("##tile", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(m_tileSize, m_tileSize + ImGui::GetTextLineHeightWithSpacing() * 2.0f)))
                {
                    context.GetSelectionService().SelectAsset(record.guid);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        OpenAsset(record.guid);
                    }
                }
                if (ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload("BRIGHT_ASSET", &record.guid, sizeof(record.guid));
                    ImGui::TextUnformatted(record.name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (record.type == Assets::AssetType::Folder && ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRIGHT_ASSET"))
                    {
                        Core::Guid dragged = *static_cast<const Core::Guid*>(payload->Data);
                        if (const Assets::AssetRecord* draggedRecord = database.FindRecord(dragged); draggedRecord && dragged != record.guid)
                        {
                            database.MoveAsset(dragged, record.path + "/" + draggedRecord->name);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                if (ImGui::BeginPopupContextItem("ItemContext"))
                {
                    context.GetSelectionService().SelectAsset(record.guid);
                    DrawItemContextMenu(record.guid);
                    ImGui::EndPopup();
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                {
                    ImGui::SetTooltip("%s\n%s", record.path.c_str(), Assets::AssetTypeName(record.type));
                }

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 imageMin(cursor.x + 4.0f, cursor.y + 4.0f);
                ImVec2 imageMax(cursor.x + m_tileSize - 4.0f, cursor.y + m_tileSize - 4.0f);
                RHI::Texture* thumbnail = record.type == Assets::AssetType::Folder ? nullptr : m_application.GetThumbnails().Get(record.guid);
                if (thumbnail)
                {
                    drawList->AddImage(ImGuiRenderer::ToTextureId(thumbnail), imageMin, imageMax);
                }
                else
                {
                    drawList->AddRectFilled(imageMin, imageMax, EditorColorU32(EditorColor::Control), 4.0f);
                    const char* glyph = TypeGlyph(record.type);
                    ImVec2 size = ImGui::CalcTextSize(glyph);
                    drawList->AddText(ImVec2((imageMin.x + imageMax.x - size.x) * 0.5f, (imageMin.y + imageMax.y - size.y) * 0.5f), TypeColor(record.type), glyph);
                }
                drawList->AddRectFilled(ImVec2(imageMin.x, imageMax.y - 3.0f), ImVec2(imageMax.x, imageMax.y), TypeColor(record.type));

                std::string label = record.name;
                if (m_renaming == record.guid)
                {
                    ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + m_tileSize));
                    ImGui::SetNextItemWidth(m_tileSize);
                    if (m_focusRename)
                    {
                        ImGui::SetKeyboardFocusHere();
                        m_focusRename = false;
                    }
                    if (ImGui::InputText("##rename", &m_renameBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                    {
                        std::string extension = record.type == Assets::AssetType::Folder ? "" : record.name.substr(std::min(record.name.find('.'), record.name.size()));
                        std::string target = (Parent(record.path).empty() ? "" : Parent(record.path) + "/") + m_renameBuffer + extension;
                        if (!database.MoveAsset(record.guid, target))
                        {
                            Core::Log::Error(Text(EditorText::RenameFailed, target));
                        }
                        m_renaming = {};
                    }
                    else if (!ImGui::IsItemActive() && !m_focusRename && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        m_renaming = {};
                    }
                }
                else
                {
                    ImVec2 textSize = ImGui::CalcTextSize(label.c_str(), nullptr, false, m_tileSize);
                    ImVec4 clip(cursor.x, cursor.y + m_tileSize, cursor.x + m_tileSize, cursor.y + m_tileSize + ImGui::GetTextLineHeightWithSpacing() * 2.0f);
                    drawList->AddText(nullptr, 0.0f, ImVec2(cursor.x + std::max(0.0f, (m_tileSize - textSize.x) * 0.5f), cursor.y + m_tileSize), ImGui::GetColorU32(ImGuiCol_Text), label.c_str(), nullptr, m_tileSize, &clip);
                }
                ImGui::EndGroup();
                ImGui::PopID();
                if (++column < columns)
                {
                    ImGui::SameLine(0.0f, 16.0f);
                }
                else
                {
                    column = 0;
                }
            }

            if (ImGui::BeginPopupContextWindow("ItemsContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                DrawCreateMenu();
                ImGui::EndPopup();
            }
            ImGui::SetCursorPos(ImVec2(0, std::max(ImGui::GetCursorPosY(), 0.0f)));
            ImGui::InvisibleButton("##dropzone", ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x), std::max(40.0f, ImGui::GetContentRegionAvail().y)));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRIGHT_OBJECT"))
                {
                    Core::Guid id = *static_cast<const Core::Guid*>(payload->Data);
                    if (Level::Level* level = context.GetLevelService().GetActiveLevel(); level && !context.GetLevelService().IsPlaying())
                    {
                        if (Level::LevelObject* object = level->FindObject(id))
                        {
                            context.GetAssetCreationService().CreateVariantAsset(*object, CurrentFolder());
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::EndChild();
            ImGui::EndTable();
        }

        if (m_pendingDelete.IsValid())
        {
            ImGui::OpenPopup(LabelWithId(EditorText::DeleteAssetQuestion, "DeleteAsset").c_str());
        }
        if (ImGui::BeginPopupModal(LabelWithId(EditorText::DeleteAssetQuestion, "DeleteAsset").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            const Assets::AssetRecord* record = database.FindRecord(m_pendingDelete);
            ImGui::TextUnformatted(Text(EditorText::DeleteAssetConfirm, record ? record->path : std::string(Text(EditorText::AssetBadgeUnknown))).c_str());
            if (ImGui::Button(Text(EditorText::Delete), ImVec2(120, 0)))
            {
                database.DeleteAsset(m_pendingDelete);
                if (context.GetSelectionService().GetSelectedAsset() == m_pendingDelete)
                {
                    context.GetSelectionService().SelectAsset({});
                }
                m_pendingDelete = {};
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(Text(EditorText::Cancel), ImVec2(120, 0)))
            {
                m_pendingDelete = {};
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        PopSelectionColors();
        ImGui::End();
    }
}
