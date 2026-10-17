#include "ValueDrawers.h"

#include "DragDrop.h"
#include "EditorText.h"
#include "Level/Level.h"
#include "Math/Color.h"
#include "Math/Quaternion.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "PropertyDrawer.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <iterator>
#include <string>
#include <unordered_map>

namespace BrightBox3D::Editor::ValueDrawers
{
    using namespace Reflection;

    namespace
    {
        using ValueDrawer = bool (*)(const char* label, const ValueDescriptor& descriptor, void* value, FieldAccess access, const Level::Level* level);

        constexpr float IntegerDragSpeed = 0.2f;
        constexpr float FloatDragSpeed = 0.01f;
        constexpr float RotationDragSpeed = 0.25f;
        constexpr float EulerCacheTolerance = 0.01f;
        constexpr const char* FloatFormat = "%.3f";
        constexpr const char* DoubleFormat = "%.4f";
        constexpr const char* RotationFormat = "%.2f";

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

        bool DrawBool(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            return ImGui::Checkbox(label, static_cast<bool*>(value));
        }

        bool DrawInt32(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            return ImGui::DragInt(label, static_cast<int32_t*>(value), IntegerDragSpeed);
        }

        bool DrawUInt32(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            return ImGui::DragScalar(label, ImGuiDataType_U32, value, IntegerDragSpeed);
        }

        bool DrawInt64(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            return ImGui::DragScalar(label, ImGuiDataType_S64, value, IntegerDragSpeed);
        }

        bool DrawFloat(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            return ImGui::DragFloat(label, static_cast<float*>(value), FloatDragSpeed, 0.0f, 0.0f, FloatFormat);
        }

        bool DrawDouble(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            return ImGui::DragScalar(label, ImGuiDataType_Double, value, FloatDragSpeed, nullptr, nullptr, DoubleFormat);
        }

        bool DrawString(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            return ImGui::InputText(label, static_cast<std::string*>(value));
        }

        bool DrawVector2(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            return ImGui::DragFloat2(label, &static_cast<Math::Vector2*>(value)->x, FloatDragSpeed, 0.0f, 0.0f, FloatFormat);
        }

        bool DrawVector3(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            return ImGui::DragFloat3(label, &static_cast<Math::Vector3*>(value)->x, FloatDragSpeed, 0.0f, 0.0f, FloatFormat);
        }

        bool DrawVector4(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            return ImGui::DragFloat4(label, &static_cast<Math::Vector4*>(value)->x, FloatDragSpeed, 0.0f, 0.0f, FloatFormat);
        }

        bool DrawQuaternion(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            auto* quaternion = static_cast<Math::Quaternion*>(value);
            auto& cache = EulerCache();
            auto it = cache.find(value);
            Math::Vector3 euler = quaternion->ToEuler();
            if (it != cache.end() && Math::Quaternion::Angle(Math::Quaternion::Euler(it->second), *quaternion) < EulerCacheTolerance)
            {
                euler = it->second;
            }
            if (!ImGui::DragFloat3(label, &euler.x, RotationDragSpeed, 0.0f, 0.0f, RotationFormat))
            {
                return false;
            }
            *quaternion = Math::Quaternion::Euler(euler);
            cache[value] = euler;
            return true;
        }

        bool DrawColor(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            return ImGui::ColorEdit4(label, &static_cast<Math::Color*>(value)->r, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        }

        bool DrawGuid(const char* label, const ValueDescriptor&, void* value, FieldAccess, const Level::Level*)
        {
            std::string text = static_cast<Core::Guid*>(value)->ToString();
            ImGui::InputText(label, &text, ImGuiInputTextFlags_ReadOnly);
            return false;
        }

        bool DrawEnum(const char* label, const ValueDescriptor& descriptor, void* value, FieldAccess, const Level::Level*)
        {
            int64_t current = ReadEnum(value, descriptor.size);
            const EnumEntry* entry = descriptor.enumInfo ? descriptor.enumInfo->FindByValue(current) : nullptr;
            if (!ImGui::BeginCombo(label, entry ? entry->name.c_str() : std::to_string(current).c_str()))
            {
                return false;
            }
            bool changed = false;
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
            return changed;
        }

        bool DrawAssetReference(const char* label, const ValueDescriptor& descriptor, void* value, FieldAccess access, const Level::Level*)
        {
            ImGui::EndDisabled();
            bool changed = PropertyDrawer::AssetField(label, descriptor.assetType, *static_cast<Core::Guid*>(value), access);
            ImGui::BeginDisabled(access == FieldAccess::ReadOnly);
            return changed;
        }

        bool DrawObjectReference(const char* label, const ValueDescriptor& descriptor, void* value, FieldAccess access, const Level::Level* level)
        {
            Core::Guid& guid = *static_cast<Core::Guid*>(value);
            const Level::LevelObject* object = level && guid.IsValid() ? level->FindObject(guid) : nullptr;
            std::string text = object ? object->GetName() : (guid.IsValid() ? std::string(Text(EditorText::MissingObject)) : Text(EditorText::NoneOfType, descriptor.referenceType));
            ImGui::InputText(label, &text, ImGuiInputTextFlags_ReadOnly);
            bool changed = false;
            if (access == FieldAccess::Editable && ImGui::BeginDragDropTarget())
            {
                if (std::optional<Core::Guid> dropped = AcceptPayload(DragDropPayload::Object))
                {
                    guid = *dropped;
                    changed = true;
                }
                ImGui::EndDragDropTarget();
            }
            if (access == FieldAccess::Editable && ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem(Text(EditorText::Clear)))
                {
                    guid = Core::Guid{};
                    changed = true;
                }
                ImGui::EndPopup();
            }
            return changed;
        }

        bool DrawClassReference(const char* label, const ValueDescriptor& descriptor, void* value, FieldAccess, const Level::Level*)
        {
            return PropertyDrawer::ClassField(label, descriptor.baseType ? &descriptor.baseType() : nullptr, *static_cast<std::string*>(value));
        }

        bool DrawArray(const char*, const ValueDescriptor&, void*, FieldAccess, const Level::Level*)
        {
            return false;
        }

        constexpr ValueDrawer Drawers[] = {
            DrawBool,
            DrawInt32,
            DrawUInt32,
            DrawInt64,
            DrawFloat,
            DrawDouble,
            DrawString,
            DrawVector2,
            DrawVector3,
            DrawVector4,
            DrawQuaternion,
            DrawColor,
            DrawGuid,
            DrawEnum,
            DrawAssetReference,
            DrawObjectReference,
            DrawClassReference,
            DrawArray
        };

        static_assert(std::size(Drawers) == static_cast<size_t>(PropertyKind::Array) + 1);
    }

    bool Draw(const char* label, const ValueDescriptor& descriptor, void* value, FieldAccess access, const Level::Level* level)
    {
        ImGui::BeginDisabled(access == FieldAccess::ReadOnly);
        bool changed = Drawers[static_cast<size_t>(descriptor.kind)](label, descriptor, value, access, level);
        ImGui::EndDisabled();
        return changed;
    }
}
