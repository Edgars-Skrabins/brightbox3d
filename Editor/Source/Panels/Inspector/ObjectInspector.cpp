#include "Panels/Inspector/ObjectInspector.h"

#include "Assets/AssetDatabase.h"
#include "EditorApplication.h"
#include "EditorPlaySession.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "Labels.h"
#include "Level/LevelSerializer.h"
#include "PropertyDrawer.h"
#include "Theme.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace BrightBox3D::Editor
{
    using Level::LevelObject;
    using Level::LevelObjectComponent;
    using Level::LevelSerializer;

    namespace
    {
        constexpr float LinkBarBackgroundAlpha = 0.12f;
        constexpr float LinkBarHeightInRows = 2.2f;
    }

    ObjectInspector::ObjectInspector(EditorApplication& application) : m_application(application), m_components(application)
    {
    }

    void ObjectInspector::RefreshSnapshot(LevelObject& object)
    {
        EditorContext& context = m_application.GetContext();
        uint64_t revision = context.GetUndoService().GetUndo().GetRevision() + context.GetLevelService().GetLevelRevision();
        if (m_cachedObject != object.GetId() || (!ImGui::IsAnyItemActive() && !m_editInProgress && m_cachedRevision != revision))
        {
            m_before.SetNull();
            ObjectEditService::Snapshot(object, m_before);
            m_cachedObject = object.GetId();
            m_cachedRevision = revision;
        }
    }

    bool ObjectInspector::DrawHeader(LevelObject& object)
    {
        bool changed = false;
        bool active = object.IsActiveSelf();
        if (ImGui::Checkbox("##active", &active))
        {
            object.SetActive(active);
            changed = true;
        }
        ImGui::SameLine();
        std::string name = object.GetName();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##name", &name))
        {
            object.SetName(name);
            changed = true;
        }
        ImGui::TextDisabled("%s", object.GetMissingTypeName().empty() ? object.GetType().GetName().c_str() : Text(EditorText::MissingTypeLabel, object.GetMissingTypeName()).c_str());
        return changed;
    }

    bool ObjectInspector::DrawLinkBar(LevelObject& object)
    {
        EditorContext& context = m_application.GetContext();
        LevelObject* linkRoot = object.GetLinkRoot();
        const Assets::AssetRecord* record = linkRoot ? Assets::AssetDatabase::Get().FindRecord(linkRoot->GetLink().asset) : nullptr;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColorVec4(EditorColor::Linked, LinkBarBackgroundAlpha));
        ImGui::BeginChild("LinkBar", ImVec2(0, ImGui::GetFrameHeightWithSpacing() * LinkBarHeightInRows), ImGuiChildFlags_Borders);
        bool linkedToTemplate = record && record->type == Assets::AssetType::LevelObjectTemplate;
        ImGui::TextColored(EditorColorVec4(EditorColor::Linked), "%s", Text(linkedToTemplate ? EditorText::LinkedTemplate : EditorText::LinkedVariant, Assets::AssetTypeName(linkedToTemplate ? Assets::AssetType::LevelObjectTemplate : Assets::AssetType::LevelObjectVariant), record ? record->name : std::string(Text(EditorText::MissingLink))).c_str());
        bool stop = false;
        if (linkRoot && !context.GetLevelService().IsPlaying())
        {
            if (ImGui::SmallButton(Text(EditorText::Select)))
            {
                context.GetSelectionService().SelectAsset(linkRoot->GetLink().asset);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(Text(EditorText::Open)))
            {
                Core::Guid asset = linkRoot->GetLink().asset;
                ImGui::EndChild();
                ImGui::PopStyleColor();
                context.GetLevelObjectEditService().OpenLevelObjectEditor(asset);
                return true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(Text(linkedToTemplate ? EditorText::ApplyToTemplate : EditorText::ApplyAll)))
            {
                LevelSerializer::ApplyAllToAsset(*linkRoot);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(Text(linkedToTemplate ? EditorText::RevertToTemplate : EditorText::RevertAll)))
            {
                Serialization::JsonDocument before;
                ObjectEditService::Snapshot(*linkRoot, before);
                Core::Guid id = linkRoot->GetId();
                LevelSerializer::RevertAll(*linkRoot);
                context.GetUndoService().RecordObjectChange(id, before, Text(EditorText::UndoRevertOverrides));
                stop = true;
            }
            else
            {
                ImGui::SameLine();
                if (ImGui::SmallButton(Text(linkedToTemplate ? EditorText::DetachFromTemplate : EditorText::Unlink)))
                {
                    Serialization::JsonDocument before;
                    ObjectEditService::Snapshot(*linkRoot, before);
                    LevelSerializer::Unlink(*linkRoot);
                    context.GetUndoService().RecordObjectChange(linkRoot->GetId(), before, Text(EditorText::Unlink));
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return stop;
    }

    void ObjectInspector::DrawKeepChanges(LevelObject& object)
    {
        if (!m_application.GetContext().GetLevelService().IsPlaying())
        {
            return;
        }
        if (EditorPlaySession* session = m_application.GetPlaySession())
        {
            bool keeping = session->IsKeepingChanges(object.GetId());
            if (ImGui::Checkbox(Text(EditorText::KeepPlayModeChanges), &keeping))
            {
                session->ToggleKeepChanges(object.GetId());
            }
        }
    }

    bool ObjectInspector::DrawObjectProperties(LevelObject& object)
    {
        EditorContext& context = m_application.GetContext();
        PropertyDrawOptions options;
        options.instanceContext = !context.GetLevelObjectEditService().IsEditingLevelObject();
        options.level = context.GetLevelService().GetActiveLevel();
        bool hasProperties = false;
        for (const Reflection::PropertyInfo* property : object.GetType().GetAllProperties())
        {
            hasProperties |= PropertyDrawer::IsVisible(*property, object, options);
        }
        if (!hasProperties)
        {
            return false;
        }
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (!ImGui::CollapsingHeader(object.GetType().GetName().c_str()))
        {
            return false;
        }
        if (object.IsLinked() && !context.GetLevelObjectEditService().IsEditingLevelObject())
        {
            AddOverrideHandlers(options, context, object, nullptr);
        }
        ImGui::Indent(ScaledMetric(EditorMetric::PropertyIndent));
        bool changed = PropertyDrawer::DrawObject(object, options);
        ImGui::Unindent(ScaledMetric(EditorMetric::PropertyIndent));
        return changed;
    }

    bool ObjectInspector::DrawComponents(LevelObject& object, ComponentEditing editing)
    {
        bool changed = false;
        size_t count = object.GetComponents().size();
        for (size_t index = 0; index < object.GetComponents().size(); ++index)
        {
            LevelObjectComponent* component = object.GetComponents()[index].get();
            if (component->IsPendingDestroy())
            {
                continue;
            }
            bool componentChanged = m_components.Draw(object, *component, index, count, editing);
            changed |= componentChanged;
            if (componentChanged && index < object.GetComponents().size() && object.GetComponents()[index].get() != component)
            {
                break;
            }
        }
        return changed;
    }

    void ObjectInspector::RecordEdit(LevelObject& object)
    {
        m_application.GetContext().GetUndoService().RecordObjectChange(object.GetId(), m_before, Text(EditorText::UndoEditProperty));
        m_editInProgress = false;
        m_cachedObject = {};
    }

    void ObjectInspector::TrackEdit(LevelObject& object, bool changed)
    {
        if (changed)
        {
            m_application.GetContext().GetLevelService().MarkDirty(object);
            if (ImGui::IsAnyItemActive())
            {
                m_editInProgress = true;
            }
            else
            {
                RecordEdit(object);
            }
        }
        else if (m_editInProgress && !ImGui::IsAnyItemActive())
        {
            RecordEdit(object);
        }
    }

    void ObjectInspector::Draw(LevelObject& object)
    {
        EditorContext& context = m_application.GetContext();
        RefreshSnapshot(object);
        bool changed = DrawHeader(object);
        ComponentEditing editing = ComponentEditing::Structure;
        if (!context.GetLevelObjectEditService().IsEditingLevelObject() && object.IsLinked())
        {
            if (DrawLinkBar(object))
            {
                return;
            }
            editing = ComponentEditing::ValuesOnly;
        }
        DrawKeepChanges(object);
        ImGui::Spacing();

        bool playing = context.GetLevelService().IsPlaying();
        if (playing)
        {
            editing = ComponentEditing::Structure;
        }
        changed |= m_components.Draw(object, object.GetTransform(), 0, 1, ComponentEditing::ValuesOnly);
        changed |= DrawObjectProperties(object);
        changed |= DrawComponents(object, editing);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (editing == ComponentEditing::Structure)
        {
            if (m_components.DrawAddComponent(object))
            {
                m_cachedObject = {};
            }
        }
        else
        {
            ImGui::TextDisabled("%s", Text(EditorText::UnlinkToAddComponents, TypeName<Level::LevelObject>()).c_str());
        }
        if (!playing)
        {
            TrackEdit(object, changed);
        }
    }
}
