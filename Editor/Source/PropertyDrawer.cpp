#include "PropertyDrawer.h"

#include "Assets/AssetDatabase.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "Level/Level.h"
#include "Math/Color.h"
#include "Math/Quaternion.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <map>
#include <unordered_map>

namespace BrightBox3D::Editor
{
    using namespace Reflection;

    namespace
    {
        std::unordered_map<const void*, Math::Vector3>& EulerCache()
        {
            static std::unordered_map<const void*, Math::Vector3> cache;
            return cache;
        }

        int64_t ReadEnum(const void* value, size_t size)
        {
            switch (size)
            {
            case 1:
                return *static_cast<const int8_t*>(value);
            case 2:
                return *static_cast<const int16_t*>(value);
            case 8:
                return *static_cast<const int64_t*>(value);
            default:
                return *static_cast<const int32_t*>(value);
            }
        }

        void WriteEnum(void* value, size_t size, int64_t number)
        {
            switch (size)
            {
            case 1:
                *static_cast<int8_t*>(value) = static_cast<int8_t>(number);
                break;
            case 2:
                *static_cast<int16_t*>(value) = static_cast<int16_t>(number);
                break;
            case 8:
                *static_cast<int64_t*>(value) = number;
                break;
            default:
                *static_cast<int32_t*>(value) = static_cast<int32_t>(number);
                break;
            }
        }

        float LabelWidth()
        {
            return std::max(110.0f, ImGui::GetContentRegionAvail().x * 0.38f);
        }
    }

    void PropertyDrawer::BeginRow(const char* label, bool bold)
    {
        ImGui::PushID(label);
        float start = ImGui::GetCursorPosX();
        ImGui::AlignTextToFramePadding();
        if (bold)
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
        if (options.readOnly || property.HasFlag(PropertyFlagVisibleAnywhere))
        {
            return false;
        }
        if (property.HasFlag(PropertyFlagEditDefaultsOnly))
        {
            return !options.instanceContext;
        }
        return property.HasFlag(PropertyFlagEditAnywhere) || property.HasFlag(PropertyFlagEditInstanceOnly);
    }

    bool PropertyDrawer::AssetField(const char* label, const std::string& assetType, Core::Guid& guid, bool readOnly)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        Assets::AssetType type = Assets::AssetTypeFromName(assetType);
        const Assets::AssetRecord* record = guid.IsValid() ? database.FindRecord(guid) : nullptr;
        std::string preview = record ? record->name : (guid.IsValid() ? Text(EditorText::MissingAsset, guid.ToString().substr(0, 8)) : Text(EditorText::NoneOfType, assetType));
        bool changed = false;
        ImGui::BeginDisabled(readOnly);
        float clearWidth = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(std::max(40.0f, ImGui::CalcItemWidth() - clearWidth - ImGui::GetStyle().ItemInnerSpacing.x));
        if (ImGui::BeginCombo(label, preview.c_str(), ImGuiComboFlags_HeightLarge))
        {
            static std::string filter;
            if (ImGui::IsWindowAppearing())
            {
                filter.clear();
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputTextWithHint("##filter", "Search", &filter);
            if (ImGui::Selectable(Text(EditorText::None), !guid.IsValid()))
            {
                guid = Core::Guid{};
                changed = true;
            }
            for (const Assets::AssetRecord& candidate : database.GetRecords(type))
            {
                if (!filter.empty())
                {
                    std::string lowerName = candidate.path;
                    std::string lowerFilter = filter;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                    if (lowerName.find(lowerFilter) == std::string::npos)
                    {
                        continue;
                    }
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
        if (!readOnly && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRIGHT_ASSET"))
            {
                Core::Guid dropped = *static_cast<const Core::Guid*>(payload->Data);
                const Assets::AssetRecord* droppedRecord = database.FindRecord(dropped);
                if (droppedRecord && (droppedRecord->type == type || type == Assets::AssetType::Unknown))
                {
                    guid = dropped;
                    changed = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::PushID("clear");
        if (ImGui::Button("x", ImVec2(clearWidth, 0.0f)) && guid.IsValid())
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

    bool PropertyDrawer::DrawValue(const char* label, const ValueDescriptor& descriptor, void* value, bool readOnly, const Level::Level* level)
    {
        bool changed = false;
        ImGui::BeginDisabled(readOnly);
        switch (descriptor.kind)
        {
        case PropertyKind::Bool:
            changed = ImGui::Checkbox(label, static_cast<bool*>(value));
            break;
        case PropertyKind::Int32:
            changed = ImGui::DragInt(label, static_cast<int32_t*>(value), 0.2f);
            break;
        case PropertyKind::UInt32:
            changed = ImGui::DragScalar(label, ImGuiDataType_U32, value, 0.2f);
            break;
        case PropertyKind::Int64:
            changed = ImGui::DragScalar(label, ImGuiDataType_S64, value, 0.2f);
            break;
        case PropertyKind::Float:
            changed = ImGui::DragFloat(label, static_cast<float*>(value), 0.01f, 0.0f, 0.0f, "%.3f");
            break;
        case PropertyKind::Double:
            changed = ImGui::DragScalar(label, ImGuiDataType_Double, value, 0.01f, nullptr, nullptr, "%.4f");
            break;
        case PropertyKind::String:
            changed = ImGui::InputText(label, static_cast<std::string*>(value));
            break;
        case PropertyKind::Vector2:
            changed = ImGui::DragFloat2(label, &static_cast<Math::Vector2*>(value)->x, 0.01f, 0.0f, 0.0f, "%.3f");
            break;
        case PropertyKind::Vector3:
            changed = ImGui::DragFloat3(label, &static_cast<Math::Vector3*>(value)->x, 0.01f, 0.0f, 0.0f, "%.3f");
            break;
        case PropertyKind::Vector4:
            changed = ImGui::DragFloat4(label, &static_cast<Math::Vector4*>(value)->x, 0.01f, 0.0f, 0.0f, "%.3f");
            break;
        case PropertyKind::Quaternion:
        {
            auto* quaternion = static_cast<Math::Quaternion*>(value);
            auto& cache = EulerCache();
            auto it = cache.find(value);
            Math::Vector3 euler = quaternion->ToEuler();
            if (it != cache.end() && Math::Quaternion::Angle(Math::Quaternion::Euler(it->second), *quaternion) < 0.01f)
            {
                euler = it->second;
            }
            if (ImGui::DragFloat3(label, &euler.x, 0.25f, 0.0f, 0.0f, "%.2f"))
            {
                *quaternion = Math::Quaternion::Euler(euler);
                cache[value] = euler;
                changed = true;
            }
            break;
        }
        case PropertyKind::Color:
            changed = ImGui::ColorEdit4(label, &static_cast<Math::Color*>(value)->r, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
            break;
        case PropertyKind::Guid:
        {
            std::string text = static_cast<Core::Guid*>(value)->ToString();
            ImGui::InputText(label, &text, ImGuiInputTextFlags_ReadOnly);
            break;
        }
        case PropertyKind::Enum:
        {
            int64_t current = ReadEnum(value, descriptor.size);
            const EnumEntry* entry = descriptor.enumInfo ? descriptor.enumInfo->FindByValue(current) : nullptr;
            if (ImGui::BeginCombo(label, entry ? entry->name.c_str() : std::to_string(current).c_str()))
            {
                if (descriptor.enumInfo)
                {
                    for (const EnumEntry& option : descriptor.enumInfo->entries)
                    {
                        if (ImGui::Selectable(option.name.c_str(), option.value == current))
                        {
                            WriteEnum(value, descriptor.size, option.value);
                            changed = true;
                        }
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }
        case PropertyKind::AssetReference:
            ImGui::EndDisabled();
            changed = AssetField(label, descriptor.referenceType, *static_cast<Core::Guid*>(value), readOnly);
            ImGui::BeginDisabled(readOnly);
            break;
        case PropertyKind::ObjectReference:
        {
            Core::Guid& guid = *static_cast<Core::Guid*>(value);
            const Level::LevelObject* object = level && guid.IsValid() ? level->FindObject(guid) : nullptr;
            std::string text = object ? object->GetName() : (guid.IsValid() ? std::string(Text(EditorText::MissingObject)) : Text(EditorText::NoneOfType, descriptor.referenceType));
            ImGui::InputText(label, &text, ImGuiInputTextFlags_ReadOnly);
            if (!readOnly && ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BRIGHT_OBJECT"))
                {
                    guid = *static_cast<const Core::Guid*>(payload->Data);
                    changed = true;
                }
                ImGui::EndDragDropTarget();
            }
            if (!readOnly && ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem(Text(EditorText::Clear)))
                {
                    guid = Core::Guid{};
                    changed = true;
                }
                ImGui::EndPopup();
            }
            break;
        }
        case PropertyKind::ClassReference:
            changed = ClassField(label, descriptor.baseType ? &descriptor.baseType() : nullptr, *static_cast<std::string*>(value));
            break;
        case PropertyKind::Array:
            break;
        }
        ImGui::EndDisabled();
        return changed;
    }

    bool PropertyDrawer::DrawProperty(const PropertyInfo& property, EngineObject& object, const PropertyDrawOptions& options)
    {
        bool editable = IsEditable(property, options);
        bool overridden = options.isOverridden && options.isOverridden(property);
        void* value = property.GetValuePointer(&object);
        bool changed = false;

        if (property.value.kind == PropertyKind::Array)
        {
            ImGui::PushID(property.name.c_str());
            size_t count = property.array.size(value);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
            if (overridden)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, EditorColorVec4(EditorColor::Override));
            }
            bool open = ImGui::TreeNodeEx("##array", flags, "%s [%zu]", property.displayName.c_str(), count);
            if (overridden)
            {
                ImGui::PopStyleColor();
            }
            if (options.onContextAction && overridden && ImGui::BeginPopupContextItem("PropertyContext"))
            {
                if (ImGui::MenuItem(Text(EditorText::RevertToLevelObject)))
                {
                    options.onContextAction(property, PropertyContextAction::Revert);
                }
                if (ImGui::MenuItem(Text(EditorText::ApplyToLevelObject)))
                {
                    options.onContextAction(property, PropertyContextAction::Apply);
                }
                ImGui::EndPopup();
            }
            if (open)
            {
                int size = static_cast<int>(count);
                BeginRow(Text(EditorText::ArraySize), false);
                ImGui::BeginDisabled(!editable);
                if (ImGui::InputInt("##size", &size, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue) && size >= 0 && size < 4096)
                {
                    property.array.resize(value, static_cast<size_t>(size));
                    changed = true;
                }
                ImGui::EndDisabled();
                EndRow();
                count = property.array.size(value);
                int removeIndex = -1;
                for (size_t i = 0; i < count; ++i)
                {
                    std::string label = Text(EditorText::ArrayElement, i);
                    BeginRow(label.c_str(), false);
                    if (editable)
                    {
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemInnerSpacing.x);
                    }
                    changed |= DrawValue("##element", property.element, property.array.element(value, i), !editable, options.level);
                    if (editable)
                    {
                        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                        if (ImGui::Button("-", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
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
                if (editable && ImGui::Button(Text(EditorText::AddElement)))
                {
                    property.array.resize(value, count + 1);
                    changed = true;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            if (changed)
            {
                object.OnPropertiesChanged();
            }
            return changed;
        }

        BeginRow(property.displayName.c_str(), overridden);
        changed = DrawValue("##value", property.value, value, !editable, options.level);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("%s", property.name.c_str());
        }
        if (options.onContextAction && overridden && ImGui::BeginPopupContextItem("PropertyContext"))
        {
            if (ImGui::MenuItem(Text(EditorText::RevertToLevelObject)))
            {
                options.onContextAction(property, PropertyContextAction::Revert);
            }
            if (ImGui::MenuItem(Text(EditorText::ApplyToLevelObject)))
            {
                options.onContextAction(property, PropertyContextAction::Apply);
            }
            ImGui::EndPopup();
        }
        EndRow();
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
