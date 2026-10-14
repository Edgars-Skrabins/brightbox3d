#include "Panels/Inspector/ComponentInspector.h"

#include "Core/EngineInfo.h"
#include "Core/StringUtils.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "Level/LevelFileSchema.h"
#include "Level/LevelSerializer.h"
#include "Level/MissingComponent.h"
#include "ObjectIcons.h"
#include "PropertyDrawer.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"
#include "Theme.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <map>
#include <vector>

namespace BrightBox3D::Editor
{
    using Level::LevelObject;
    using Level::LevelObjectComponent;
    using Level::LevelSerializer;

    namespace
    {

        struct TransformLabel
        {
            const char* property;
            EditorText label;
        };

        constexpr TransformLabel TransformLabels[] = {
            { "m_localPosition", EditorText::Position },
            { "m_localRotation", EditorText::Rotation },
            { "m_localScale", EditorText::Scale }
        };

        Reflection::PropertyInfo TransformDisplay(const Reflection::PropertyInfo& property)
        {
            Reflection::PropertyInfo display = property;
            for (const TransformLabel& entry : TransformLabels)
            {
                if (display.name == entry.property)
                {
                    display.displayName = Text(entry.label);
                }
            }
            return display;
        }

        bool DrawTransformProperties(Level::Transform& transform, PropertyDrawOptions& options)
        {
            bool changed = false;
            options.filter = [](const Reflection::PropertyInfo&) { return true; };
            for (const Reflection::PropertyInfo* property : transform.GetType().GetAllProperties())
            {
                changed |= PropertyDrawer::DrawProperty(TransformDisplay(*property), transform, options);
            }
            return changed;
        }

        std::string ComponentCategory(const Reflection::TypeInfo& type)
        {
            if (!type.GetCategory().empty())
            {
                return type.GetCategory();
            }
            return type.GetModuleName() == Core::EngineModuleName ? Text(EditorText::EngineCategory) : Text(EditorText::GameCategory);
        }
    }

    void AddOverrideHandlers(PropertyDrawOptions& options, EditorContext& context, LevelObject& object, LevelObjectComponent* component)
    {
        options.isOverridden = [&object, component](const Reflection::PropertyInfo& property)
        {
            return LevelSerializer::IsPropertyOverridden(object, component, property.name);
        };
        options.onContextAction = [&context, &object, component](const Reflection::PropertyInfo& property, PropertyContextAction action)
        {
            if (action == PropertyContextAction::Revert)
            {
                Serialization::JsonDocument before;
                ObjectEditService::Snapshot(object, before);
                LevelSerializer::RevertProperty(object, component, property.name);
                context.GetUndoService().RecordObjectChange(object.GetId(), before, Text(EditorText::UndoRevertProperty));
            }
            else
            {
                LevelSerializer::ApplyPropertyToAsset(object, component, property.name);
            }
        };
    }

    ComponentInspector::ComponentInspector(EditorApplication& application) : m_application(application)
    {
    }

    bool ComponentInspector::DrawContextMenu(LevelObject& object, LevelObjectComponent& component, size_t index, size_t count, bool structural, bool& removed)
    {
        if (!ImGui::BeginPopupContextItem("ComponentContext"))
        {
            return false;
        }
        bool changed = false;
        ImGui::BeginDisabled(!structural);
        if (ImGui::MenuItem(Text(EditorText::RemoveComponent)))
        {
            removed = true;
        }
        if (ImGui::MenuItem(Text(EditorText::MoveUp), nullptr, false, index > 0))
        {
            object.MoveComponent(&component, static_cast<int>(index) - 1);
            changed = true;
        }
        if (ImGui::MenuItem(Text(EditorText::MoveDown), nullptr, false, index + 1 < count))
        {
            object.MoveComponent(&component, static_cast<int>(index) + 1);
            changed = true;
        }
        ImGui::EndDisabled();
        if (ImGui::MenuItem(Text(EditorText::Reset)))
        {
            std::unique_ptr<Reflection::EngineObject> fresh(component.GetType().CreateInstance());
            if (fresh)
            {
                Serialization::CopyProperties(*fresh, component);
                changed = true;
            }
        }
        ImGui::EndPopup();
        return changed;
    }

    bool ComponentInspector::DrawEnabledToggle(LevelObjectComponent& component)
    {
        bool enabled = component.IsEnabled();
        bool changed = ImGui::Checkbox("##enabled", &enabled);
        if (changed)
        {
            component.SetEnabled(enabled);
        }
        ImGui::SameLine();
        return changed;
    }

    bool ComponentInspector::DrawProperties(LevelObject& object, LevelObjectComponent& component)
    {
        EditorContext& context = m_application.GetContext();
        if (auto* missing = Reflection::Cast<Level::MissingComponent>(&component))
        {
            ImGui::TextWrapped("%s", Text(EditorText::MissingComponentExplanation, missing->GetMissingTypeName()).c_str());
            return false;
        }
        PropertyDrawOptions options;
        options.instanceContext = !context.GetLevelObjectEditService().IsEditingLevelObject();
        options.level = context.GetLevelService().GetActiveLevel();
        if (object.IsLinked() && !context.GetLevelObjectEditService().IsEditingLevelObject())
        {
            AddOverrideHandlers(options, context, object, &component);
        }
        if (&component == &object.GetTransform())
        {
            return DrawTransformProperties(object.GetTransform(), options);
        }
        return PropertyDrawer::DrawObject(component, options);
    }

    bool ComponentInspector::Draw(LevelObject& object, LevelObjectComponent& component, size_t index, size_t count, bool structural)
    {
        bool changed = false;
        ImGui::PushID(component.GetLocalId().ToString().c_str());
        bool isTransform = &component == &object.GetTransform();
        auto* missing = Reflection::Cast<Level::MissingComponent>(&component);
        std::string title = missing ? Text(EditorText::MissingComponentTitle, missing->GetMissingTypeName()) : ObjectIcons::Label(component.GetType(), component.GetType().GetName());

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        bool open = ImGui::CollapsingHeader(("##header" + title).c_str(), ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth);
        bool removed = false;
        if (!isTransform)
        {
            changed |= DrawContextMenu(object, component, index, count, structural, removed);
        }
        ImGui::SameLine(ImGui::GetStyle().IndentSpacing + ImGui::GetStyle().FramePadding.x * 2.0f);
        if (!isTransform)
        {
            changed |= DrawEnabledToggle(component);
        }
        bool overriddenEnabled = !isTransform && LevelSerializer::IsPropertyOverridden(object, &component, Level::LevelFileSchema::PseudoEnabled);
        if (overriddenEnabled)
        {
            ImGui::TextColored(EditorColorVec4(EditorColor::Linked), "%s", title.c_str());
        }
        else
        {
            ImGui::TextUnformatted(title.c_str());
        }

        if (open)
        {
            ImGui::Indent(ScaledMetric(EditorMetric::PropertyIndent));
            changed |= DrawProperties(object, component);
            ImGui::Unindent(ScaledMetric(EditorMetric::PropertyIndent));
        }
        ImGui::PopID();
        if (removed)
        {
            object.RemoveComponent(&component);
            changed = true;
        }
        return changed;
    }

    bool ComponentInspector::DrawAddComponent(LevelObject& object)
    {
        float width = std::min(ScaledMetric(EditorMetric::AddComponentButtonWidth), ImGui::GetContentRegionAvail().x);
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - width) * 0.5f + ImGui::GetCursorPosX());
        if (ImGui::Button(Text(EditorText::AddComponent), ImVec2(width, 0.0f)))
        {
            m_filter.clear();
            ImGui::OpenPopup("AddComponentPopup");
        }
        if (!ImGui::BeginPopup("AddComponentPopup"))
        {
            return false;
        }
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::InputTextWithHint("##search", Text(EditorText::SearchComponentsHint), &m_filter);
        std::string filter = Core::ToLower(m_filter);
        std::map<std::string, std::vector<const Reflection::TypeInfo*>> byCategory;
        for (const Reflection::TypeInfo* type : Reflection::TypeRegistry::GetDerived(LevelObjectComponent::StaticType()))
        {
            if (type == &Level::Transform::StaticType() || type == &Level::MissingComponent::StaticType())
            {
                continue;
            }
            if (!filter.empty() && Core::ToLower(type->GetName()).find(filter) == std::string::npos)
            {
                continue;
            }
            byCategory[ComponentCategory(*type)].push_back(type);
        }
        bool added = false;
        for (auto& [category, types] : byCategory)
        {
            ImGui::SeparatorText(category.c_str());
            for (const Reflection::TypeInfo* type : types)
            {
                if (ImGui::Selectable(ObjectIcons::Label(*type, type->GetName()).c_str()))
                {
                    Serialization::JsonDocument before;
                    ObjectEditService::Snapshot(object, before);
                    object.AddComponent(*type);
                    m_application.GetContext().GetUndoService().RecordObjectChange(object.GetId(), before, Text(EditorText::UndoAddComponent, type->GetName()));
                    added = true;
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndPopup();
        return added;
    }
}
