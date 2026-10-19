#include "Panels/Assets/EngineClassesView.h"

#include "Assets/AssetType.h"
#include "Core/EngineInfo.h"
#include "Core/FileSystem.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "ObjectIcons.h"
#include "Panels/Assets/AssetTypeStyle.h"
#include "Reflection/TypeRegistry.h"
#include "Theme.h"

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace BrightBox3D::Editor::EngineClassesView
{
    namespace
    {
        constexpr float TileSpacing = 16.0f;
        constexpr float ThumbnailInset = 4.0f;
        constexpr float ThumbnailRounding = 4.0f;
        constexpr float LabelLines = 2.0f;
        constexpr float TileIconScale = 0.55f;

        using ModuleClasses = std::map<std::string, std::vector<const Reflection::TypeInfo*>>;

        std::string ModuleOf(const Reflection::TypeInfo& type)
        {
            std::filesystem::path header(type.GetHeaderPath());
            return header.has_parent_path() ? header.begin()->string() : std::string();
        }

        ModuleClasses CollectModules()
        {
            ModuleClasses modules;
            for (const Reflection::TypeInfo* type : Reflection::TypeRegistry::GetAll())
            {
                if (type->GetModuleName() == Core::EngineModuleName && !type->GetHeaderPath().empty())
                {
                    modules[ModuleOf(*type)].push_back(type);
                }
            }
            for (auto& [module, types] : modules)
            {
                std::sort(types.begin(), types.end(), [](const Reflection::TypeInfo* a, const Reflection::TypeInfo* b) { return a->GetName() < b->GetName(); });
            }
            return modules;
        }

        Icon ClassIcon(const Reflection::TypeInfo& type)
        {
            Icon icon = ObjectIcons::TypeIcon(type);
            return icon == Icon::None ? Icon::FileCode : icon;
        }

        std::optional<std::filesystem::path> HeaderFile(const Reflection::TypeInfo& type)
        {
            std::optional<std::filesystem::path> source = Core::FileSystem::EngineSourceDirectory();
            if (!source)
            {
                return std::nullopt;
            }
            std::filesystem::path header = *source / type.GetHeaderPath();
            std::error_code error;
            return std::filesystem::exists(header, error) ? std::optional<std::filesystem::path>(header) : std::nullopt;
        }

        void OpenModule(AssetsPanelState& state, const std::string& module)
        {
            state.root = AssetsRoot::EngineClasses;
            state.engineModule = module;
        }

        void HandleClassInput(EditorApplication& application, AssetsPanelState& state, const Reflection::TypeInfo& type)
        {
            state.selectedEngineClass = type.GetName();
            if (!ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                return;
            }
            if (std::optional<std::filesystem::path> header = HeaderFile(type))
            {
                application.OpenFileInEditor(header->string(), 0);
            }
        }

        void DrawClassTooltip(const Reflection::TypeInfo& type)
        {
            if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            {
                return;
            }
            if (!HeaderFile(type))
            {
                ImGui::SetTooltip("%s", Text(EditorText::EngineSourceMissing));
                return;
            }
            const Reflection::TypeInfo* base = type.GetBase();
            ImGui::SetTooltip("%s", Text(EditorText::EngineClassTooltip, type.GetHeaderPath(), base ? base->GetName() : std::string()).c_str());
        }

        void DrawTile(const std::string& id, const std::string& label, Icon icon, EditorColor color, bool selected, float tileSize)
        {
            ImGui::PushID(id.c_str());
            ImGui::BeginGroup();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::Selectable("##tile", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(tileSize, tileSize + ImGui::GetTextLineHeightWithSpacing() * LabelLines));
            ImGui::PopID();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 imageMin(cursor.x + ThumbnailInset, cursor.y + ThumbnailInset);
            ImVec2 imageMax(cursor.x + tileSize - ThumbnailInset, cursor.y + tileSize - ThumbnailInset);
            drawList->AddRectFilled(imageMin, imageMax, EditorColorU32(EditorColor::Control), ThumbnailRounding);
            DrawCenteredIcon(icon, imageMin, imageMax, tileSize * TileIconScale, EditorColorU32(color));
            ImVec2 textSize = ImGui::CalcTextSize(label.c_str(), nullptr, false, tileSize);
            ImVec4 clip(cursor.x, cursor.y + tileSize, cursor.x + tileSize, cursor.y + tileSize + ImGui::GetTextLineHeightWithSpacing() * LabelLines);
            drawList->AddText(nullptr, 0.0f, ImVec2(cursor.x + std::max(0.0f, (tileSize - textSize.x) * 0.5f), cursor.y + tileSize), ImGui::GetColorU32(ImGuiCol_Text), label.c_str(), nullptr, tileSize, &clip);
            ImGui::EndGroup();
        }

        void DrawRow(const std::string& id, const std::string& label, Icon icon, EditorColor color, bool selected, const std::string& detail)
        {
            ImGui::PushID(id.c_str());
            ImVec2 rowPosition = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;
            ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(width, ImGui::GetFrameHeight()));
            ImGui::PopID();
            ImVec2 labelPosition(rowPosition.x + ImGui::GetStyle().FramePadding.x, rowPosition.y + ImGui::GetStyle().FramePadding.y);
            DrawIconLabel(labelPosition, icon, color, label);
            float detailX = rowPosition.x + width - ImGui::GetStyle().FramePadding.x - ImGui::CalcTextSize(detail.c_str()).x;
            ImGui::GetWindowDrawList()->AddText(ImVec2(detailX, labelPosition.y), EditorColorU32(EditorColor::TextDim), detail.c_str());
        }

        void DrawEntry(const std::string& id, const std::string& label, Icon icon, EditorColor color, bool selected, const std::string& detail, float tileSize)
        {
            if (EditorPreferences::Get().IsAssetListMode())
            {
                DrawRow(id, label, icon, color, selected, detail);
            }
            else
            {
                DrawTile(id, label, icon, color, selected, tileSize);
            }
        }

        bool DrawNode(const std::string& id, const std::string& label, Icon icon, EditorColor color, ImGuiTreeNodeFlags flags)
        {
            ImVec2 rowPosition = ImGui::GetCursorScreenPos();
            ImVec2 labelPosition(rowPosition.x + ImGui::GetTreeNodeToLabelSpacing(), rowPosition.y + ImGui::GetStyle().FramePadding.y);
            bool open = ImGui::TreeNodeEx(id.c_str(), flags, "%s", "");
            DrawIconLabel(labelPosition, icon, color, label);
            return open;
        }

        ImGuiTreeNodeFlags FolderFlags(bool selected)
        {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            return selected ? flags | ImGuiTreeNodeFlags_Selected : flags;
        }

        const AssetTypeStyle& FolderStyle()
        {
            return GetAssetTypeStyle(Assets::AssetType::Folder);
        }
    }

    void DrawFolderTree(AssetsPanelState& state)
    {
        bool rootSelected = state.root == AssetsRoot::EngineClasses && state.engineModule.empty();
        bool open = DrawNode("##engineclasses", Text(EditorText::EngineClassesTitle), Icon::FolderCode, FolderStyle().color, FolderFlags(rootSelected));
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            OpenModule(state, {});
        }
        if (!open)
        {
            return;
        }
        for (const auto& [module, types] : CollectModules())
        {
            bool selected = state.root == AssetsRoot::EngineClasses && state.engineModule == module;
            DrawNode("##module" + module, module, FolderStyle().icon, FolderStyle().color, FolderFlags(selected) | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
            if (ImGui::IsItemClicked())
            {
                OpenModule(state, module);
            }
        }
        ImGui::TreePop();
    }

    void DrawBreadcrumbs(AssetsPanelState& state)
    {
        ImGui::AlignTextToFramePadding();
        if (ImGui::Selectable(Text(EditorText::EngineClassesTitle), false, ImGuiSelectableFlags_None, ImGui::CalcTextSize(Text(EditorText::EngineClassesTitle))))
        {
            state.engineModule.clear();
        }
        if (state.engineModule.empty())
        {
            return;
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextDisabled("%s", IconGlyph(Icon::ChevronRight));
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(state.engineModule.c_str());
    }

    void DrawItems(EditorApplication& application, AssetsPanelState& state)
    {
        ModuleClasses modules = CollectModules();
        float tileSize = static_cast<float>(EditorPreferences::Get().GetAssetIconSize());
        int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (tileSize + TileSpacing)));
        int column = 0;
        auto nextColumn = [&]()
        {
            if (EditorPreferences::Get().IsAssetListMode())
            {
                return;
            }
            if (++column < columns)
            {
                ImGui::SameLine(0.0f, TileSpacing);
            }
            else
            {
                column = 0;
            }
        };
        if (state.engineModule.empty())
        {
            for (const auto& [module, types] : modules)
            {
                DrawEntry("##module" + module, module, FolderStyle().icon, FolderStyle().color, false, std::to_string(types.size()), tileSize);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    OpenModule(state, module);
                }
                nextColumn();
            }
            return;
        }
        auto found = modules.find(state.engineModule);
        if (found == modules.end())
        {
            return;
        }
        for (const Reflection::TypeInfo* type : found->second)
        {
            const Reflection::TypeInfo* base = type->GetBase();
            DrawEntry("##class" + type->GetName(), type->GetName(), ClassIcon(*type), EditorColor::AssetScript, state.selectedEngineClass == type->GetName(), base ? base->GetName() : std::string(), tileSize);
            if (ImGui::IsItemClicked())
            {
                HandleClassInput(application, state, *type);
            }
            DrawClassTooltip(*type);
            nextColumn();
        }
    }

    void DrawTree(EditorApplication& application, AssetsPanelState& state)
    {
        bool open = DrawNode("##engineclasses", Text(EditorText::EngineClassesTitle), Icon::FolderCode, FolderStyle().color, FolderFlags(false));
        if (!open)
        {
            return;
        }
        for (const auto& [module, types] : CollectModules())
        {
            if (!DrawNode("##module" + module, module, FolderStyle().icon, FolderStyle().color, FolderFlags(false)))
            {
                continue;
            }
            for (const Reflection::TypeInfo* type : types)
            {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (state.selectedEngineClass == type->GetName())
                {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }
                DrawNode("##class" + type->GetName(), type->GetName(), ClassIcon(*type), EditorColor::AssetScript, flags);
                if (ImGui::IsItemClicked())
                {
                    HandleClassInput(application, state, *type);
                }
                DrawClassTooltip(*type);
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
}
