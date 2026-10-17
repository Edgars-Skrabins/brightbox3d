#include "PropertyDrawer.h"

#include "Assets/AssetDatabase.h"
#include "Core/StringUtils.h"
#include "DragDrop.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "Labels.h"
#include "Level/LevelObject.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"
#include "Theme.h"
#include "ValueDrawers.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <map>

namespace BrightBox3D::Editor
{
    using namespace Reflection;

    namespace
    {
        constexpr float LabelWidthRatio = 0.38f;
        constexpr int MaxArraySize = 4096;

        float LabelWidth()
        {
            return std::max(ScaledMetric(EditorMetric::PropertyLabelMinWidth), ImGui::GetContentRegionAvail().x * LabelWidthRatio);
        }
    }

    void PropertyDrawer::BeginRow(const char* label, RowLabel style)
    {
        ImGui::PushID(label);
        float start = ImGui::GetCursorPosX();
        ImGui::AlignTextToFramePadding();
        if (style == RowLabel::Bold)
        {
            ImGui::TextColored(EditorColorVec4(EditorColor::Override), "%s", label);
        }
        else
        {
            ImGui::TextUnformatted(label);
        }
        ImGui::SameLine(start + LabelWidth());
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    void PropertyDrawer::EndRow()
    {
        ImGui::PopID();
    }

    bool PropertyDrawer::IsVisible(const PropertyInfo& property, const PropertyDrawOptions& options)
    {
        if (options.filter && !options.filter(property))
        {
            return false;
        }
        if (property.HasFlag(PropertyFlagHidden) || !property.HasFlag(PropertyFlagEditorVisible))
        {
            return false;
        }
        if (property.HasFlag(PropertyFlagEditInstanceOnly) && !options.instanceContext)
        {
            return false;
        }
        return true;
    }

    bool PropertyDrawer::IsEditable(const PropertyInfo& property, const PropertyDrawOptions& options)
    {
        if (options.access == FieldAccess::ReadOnly || property.HasFlag(PropertyFlagVisibleAnywhere))
        {
            return false;
        }
        if (property.HasFlag(PropertyFlagEditDefaultsOnly))
        {
            return !options.instanceContext;
        }
        return property.HasFlag(PropertyFlagEditAnywhere) || property.HasFlag(PropertyFlagEditInstanceOnly);
    }

    bool PropertyDrawer::AssetField(const char* label, Assets::AssetType type, Core::Guid& guid, FieldAccess access)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        const Assets::AssetRecord* record = guid.IsValid() ? database.FindRecord(guid) : nullptr;
        std::string preview = record ? record->name : (guid.IsValid() ? Text(EditorText::MissingAsset, guid.ToString().substr(0, 8)) : Text(EditorText::NoneOfType, Assets::AssetTypeName(type)));
        bool changed = false;
        ImGui::BeginDisabled(access == FieldAccess::ReadOnly);
        float clearWidth = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(std::max(ScaledMetric(EditorMetric::AssetFieldMinWidth), ImGui::CalcItemWidth() - clearWidth - ImGui::GetStyle().ItemInnerSpacing.x));
        if (ImGui::BeginCombo(label, preview.c_str(), ImGuiComboFlags_HeightLarge))
        {
            static std::string filter;
            if (ImGui::IsWindowAppearing())
            {
                filter.clear();
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputTextWithHint("##filter", Text(EditorText::SearchHint), &filter);
            if (ImGui::Selectable(Text(EditorText::None), !guid.IsValid()))
            {
                guid = Core::Guid{};
                changed = true;
            }
            std::string lowerFilter = Core::ToLower(filter);
            for (const Assets::AssetRecord& candidate : database.GetRecords(type))
            {
                if (!lowerFilter.empty() && Core::ToLower(candidate.path).find(lowerFilter) == std::string::npos)
                {
                    continue;
                }
                ImGui::PushID(candidate.guid.ToString().c_str());
                if (ImGui::Selectable(candidate.name.c_str(), candidate.guid == guid))
                {
                    guid = candidate.guid;
                    changed = true;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", candidate.path.c_str());
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        if (access == FieldAccess::Editable && ImGui::BeginDragDropTarget())
        {
            if (std::optional<Core::Guid> dropped = AcceptPayload(DragDropPayload::Asset))
            {
                const Assets::AssetRecord* droppedRecord = database.FindRecord(*dropped);
                if (droppedRecord && (droppedRecord->type == type || type == Assets::AssetType::Unknown))
                {
                    guid = *dropped;
                    changed = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::PushID("clear");
        if (ImGui::Button(Text(EditorText::ClearFieldButton), ImVec2(clearWidth, 0.0f)) && guid.IsValid())
        {
            guid = Core::Guid{};
            changed = true;
        }
        ImGui::PopID();
        ImGui::EndDisabled();
        return changed;
    }

    bool PropertyDrawer::ClassField(const char* label, const Reflection::TypeInfo* baseType, std::string& typeName)
    {
        bool changed = false;
        if (ImGui::BeginCombo(label, typeName.c_str()))
        {
            if (baseType)
            {
                for (const Reflection::TypeInfo* type : Reflection::TypeRegistry::GetDerived(*baseType))
                {
                    if (ImGui::Selectable(type->GetName().c_str(), type->GetName() == typeName))
                    {
                        typeName = type->GetName();
                        changed = true;
                    }
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool PropertyDrawer::DrawValue(const char* label, const ValueDescriptor& descriptor, void* value, FieldAccess access, const Level::Level* level)
    {
        return ValueDrawers::Draw(label, descriptor, value, access, level);
    }

    void PropertyDrawer::DrawOverrideMenu(const PropertyInfo& property, const PropertyDrawOptions& options, bool overridden)
    {
        if (!options.onContextAction || !overridden || !ImGui::BeginPopupContextItem("PropertyContext"))
        {
            return;
        }
        if (ImGui::MenuItem(Text(EditorText::RevertToLevelObject, TypeName<Level::LevelObject>()).c_str()))
        {
            options.onContextAction(property, PropertyContextAction::Revert);
        }
        if (ImGui::MenuItem(Text(EditorText::ApplyToLevelObject, TypeName<Level::LevelObject>()).c_str()))
        {
            options.onContextAction(property, PropertyContextAction::Apply);
        }
        ImGui::EndPopup();
    }

    bool PropertyDrawer::DrawArrayElements(const PropertyInfo& property, EngineObject& object, const PropertyDrawOptions& options, FieldAccess access)
    {
        void* value = property.GetValuePointer(&object);
        bool changed = false;
        int size = static_cast<int>(property.array.size(value));
        BeginRow(Text(EditorText::ArraySize), RowLabel::Normal);
        ImGui::BeginDisabled(access == FieldAccess::ReadOnly);
        if (ImGui::InputInt("##size", &size, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue) && size >= 0 && size < MaxArraySize)
        {
            property.array.resize(value, static_cast<size_t>(size));
            changed = true;
        }
        ImGui::EndDisabled();
        EndRow();
        size_t count = property.array.size(value);
        int removeIndex = -1;
        for (size_t i = 0; i < count; ++i)
        {
            std::string label = Text(EditorText::ArrayElement, i);
            BeginRow(label.c_str(), RowLabel::Normal);
            if (access == FieldAccess::Editable)
            {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemInnerSpacing.x);
            }
            changed |= DrawValue("##element", property.element, property.array.element(value, i), access, options.level);
            if (access == FieldAccess::Editable)
            {
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button(Text(EditorText::RemoveElementButton), ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                {
                    removeIndex = static_cast<int>(i);
                }
            }
            EndRow();
        }
        if (removeIndex >= 0)
        {
            Serialization::JsonDocument document;
            Serialization::JsonValue array;
            Serialization::WriteProperty(property, object, array, document.GetAllocator());
            if (array.IsArray() && removeIndex < static_cast<int>(array.Size()))
            {
                array.Erase(array.Begin() + removeIndex);
                Serialization::ReadProperty(property, object, array);
            }
            changed = true;
        }
        if (access == FieldAccess::Editable && ImGui::Button(Text(EditorText::AddElement)))
        {
            property.array.resize(value, count + 1);
            changed = true;
        }
        return changed;
    }

    bool PropertyDrawer::DrawArrayProperty(const PropertyInfo& property, EngineObject& object, const PropertyDrawOptions& options, FieldAccess access, bool overridden)
    {
        ImGui::PushID(property.name.c_str());
        size_t count = property.array.size(property.GetValuePointer(&object));
        if (overridden)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColorVec4(EditorColor::Override));
        }
        bool open = ImGui::TreeNodeEx("##array", ImGuiTreeNodeFlags_SpanAvailWidth, "%s [%zu]", property.displayName.c_str(), count);
        if (overridden)
        {
            ImGui::PopStyleColor();
        }
        DrawOverrideMenu(property, options, overridden);
        bool changed = false;
        if (open)
        {
            changed = DrawArrayElements(property, object, options, access);
            ImGui::TreePop();
        }
        ImGui::PopID();
        return changed;
    }

    bool PropertyDrawer::DrawProperty(const PropertyInfo& property, EngineObject& object, const PropertyDrawOptions& options)
    {
        FieldAccess access = IsEditable(property, options) ? FieldAccess::Editable : FieldAccess::ReadOnly;
        bool overridden = options.isOverridden && options.isOverridden(property);
        bool changed = false;
        if (property.value.kind == PropertyKind::Array)
        {
            changed = DrawArrayProperty(property, object, options, access, overridden);
        }
        else
        {
            BeginRow(property.displayName.c_str(), overridden ? RowLabel::Bold : RowLabel::Normal);
            changed = DrawValue("##value", property.value, property.GetValuePointer(&object), access, options.level);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("%s", property.name.c_str());
            }
            DrawOverrideMenu(property, options, overridden);
            EndRow();
        }
        if (changed)
        {
            object.OnPropertiesChanged();
        }
        return changed;
    }

    bool PropertyDrawer::DrawObject(EngineObject& object, const PropertyDrawOptions& options)
    {
        std::vector<const PropertyInfo*> properties = object.GetType().GetAllProperties();
        std::vector<std::string> categories;
        std::map<std::string, std::vector<const PropertyInfo*>> grouped;
        for (const PropertyInfo* property : properties)
        {
            if (!IsVisible(*property, options))
            {
                continue;
            }
            if (!grouped.count(property->category))
            {
                categories.push_back(property->category);
            }
            grouped[property->category].push_back(property);
        }
        bool changed = false;
        for (const std::string& category : categories)
        {
            if (!category.empty() && categories.size() > 1)
            {
                ImGui::SeparatorText(category.c_str());
            }
            for (const PropertyInfo* property : grouped[category])
            {
                changed |= DrawProperty(*property, object, options);
            }
        }
        return changed;
    }
}
