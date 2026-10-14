#include "Panels/InspectorPanel.h"

#include "Assets/AssetDatabase.h"
#include "Assets/TextAsset.h"
#include "Core/EngineInfo.h"
#include "Core/Log.h"
#include "EditorApplication.h"
#include "EditorPlaySession.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "EditorWindows.h"
#include "Framework/LevelGameplaySettings.h"
#include "Geometry/ModelAsset.h"
#include "ImGui/ImGuiRenderer.h"
#include "Level/LevelFileSchema.h"
#include "Level/LevelSerializer.h"
#include "Level/MissingComponent.h"
#include "ObjectIcons.h"
#include "Panels/LevelViewPanel.h"
#include "PropertyDrawer.h"
#include "Reflection/PropertyTraits.h"
#include "Reflection/TypeRegistry.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/TextureAsset.h"
#include "Serialization/ObjectSerializer.h"
#include "Thumbnails.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <map>

namespace BrightBox3D::Editor
{
    using Level::LevelObjectComponent;
    using Level::LevelObject;
    using Level::LevelSerializer;

    InspectorPanel::InspectorPanel(EditorApplication& application) : m_application(application)
    {
    }

    InspectorPanel::~InspectorPanel() = default;

    void InspectorPanel::TrackEdit(LevelObject& object, bool changed)
    {
        EditorContext& context = m_application.GetContext();
        if (changed)
        {
            context.GetLevelService().MarkDirty(object);
            if (ImGui::IsAnyItemActive())
            {
                m_editInProgress = true;
            }
            else
            {
                context.GetUndoService().RecordObjectChange(object.GetId(), m_before, Text(EditorText::UndoEditProperty));
                m_editInProgress = false;
                m_cachedObject = {};
            }
        }
        else if (m_editInProgress && !ImGui::IsAnyItemActive())
        {
            context.GetUndoService().RecordObjectChange(object.GetId(), m_before, Text(EditorText::UndoEditProperty));
            m_editInProgress = false;
            m_cachedObject = {};
        }
    }

    bool InspectorPanel::DrawGameplaySettings(Serialization::JsonDocument& levelSettings, Framework::GameplaySlot slot, bool readOnly)
    {
        using Framework::LevelGameplaySettings;
        const Reflection::TypeInfo& base = LevelGameplaySettings::GetBaseType(slot);
        auto& allocator = levelSettings.GetAllocator();
        Serialization::JsonValue& entry = LevelGameplaySettings::GetOrCreateEntry(levelSettings, slot);
        std::string className = LevelGameplaySettings::GetClassName(entry);
        bool changed = false;
        PropertyDrawer::BeginRow(Text(EditorText::ClassRowLabel, base.GetName()).c_str(), false);
        ImGui::BeginDisabled(readOnly);
        if (ImGui::BeginCombo("##class", className.empty() ? "None" : className.c_str()))
        {
            if (LevelGameplaySettings::IsOptional(slot) && ImGui::Selectable(Text(EditorText::None), className.empty()))
            {
                className.clear();
                changed = true;
            }
            for (const Reflection::TypeInfo* type : Reflection::TypeRegistry::GetDerived(base))
            {
                if (ImGui::Selectable(type->GetName().c_str(), type->GetName() == className))
                {
                    className = type->GetName();
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        PropertyDrawer::EndRow();
        if (changed)
        {
            LevelGameplaySettings::SetClassName(entry, className, allocator);
        }
        if (className.empty())
        {
            return changed;
        }
        const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(className);
        if (!type || !type->IsA(base) || type->IsAbstract())
        {
            ImGui::TextColored(EditorColorVec4(EditorColor::Missing), "%s", Text(EditorText::ClassNotLoaded, base.GetName(), className).c_str());
            return changed;
        }
        Serialization::JsonValue& settings = LevelGameplaySettings::GetObjectSettings(entry, allocator);
        std::unique_ptr<Reflection::EngineObject> temporary(type->CreateInstance());
        Serialization::DeserializeProperties(*temporary, settings);
        PropertyDrawOptions options;
        options.instanceContext = false;
        options.readOnly = readOnly;
        if (PropertyDrawer::DrawObject(*temporary, options))
        {
            Serialization::JsonValue updated;
            Serialization::SerializeProperties(*temporary, updated, allocator);
            settings = std::move(updated);
            changed = true;
        }
        return changed;
    }

    bool InspectorPanel::DrawComponent(LevelObject& object, LevelObjectComponent& component, size_t index, size_t count, bool structural)
    {
        EditorContext& context = m_application.GetContext();
        bool changed = false;
        ImGui::PushID(component.GetLocalId().ToString().c_str());
        bool isTransform = &component == &object.GetTransform();
        auto* missing = Reflection::Cast<Level::MissingComponent>(&component);
        std::string title = missing ? Text(EditorText::MissingComponentTitle, missing->GetMissingTypeName()) : ObjectIcons::Label(component.GetType(), component.GetType().GetName());

        float checkboxSpace = isTransform ? 0.0f : ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        bool open = ImGui::CollapsingHeader(("##header" + title).c_str(), ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth);
        bool removed = false;
        if (!isTransform && ImGui::BeginPopupContextItem("ComponentContext"))
        {
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
        }
        ImGui::SameLine(ImGui::GetStyle().IndentSpacing + ImGui::GetStyle().FramePadding.x * 2.0f);
        if (!isTransform)
        {
            bool enabled = component.IsEnabled();
            if (ImGui::Checkbox("##enabled", &enabled))
            {
                component.SetEnabled(enabled);
                changed = true;
            }
            ImGui::SameLine();
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
        (void)checkboxSpace;

        if (open)
        {
            ImGui::Indent(4.0f);
            if (missing)
            {
                ImGui::TextWrapped("%s", Text(EditorText::MissingComponentExplanation, missing->GetMissingTypeName()).c_str());
            }
            else
            {
                PropertyDrawOptions options;
                options.instanceContext = !context.GetLevelObjectEditService().IsEditingLevelObject();
                options.level = context.GetLevelService().GetActiveLevel();
                if (object.IsLinked() && !context.GetLevelObjectEditService().IsEditingLevelObject())
                {
                    options.isOverridden = [&](const Reflection::PropertyInfo& property)
                    {
                        return LevelSerializer::IsPropertyOverridden(object, &component, property.name);
                    };
                    options.onContextAction = [&](const Reflection::PropertyInfo& property, PropertyContextAction action)
                    {
                        if (action == PropertyContextAction::Revert)
                        {
                            Serialization::JsonDocument before;
                            ObjectEditService::Snapshot(object, before);
                            LevelSerializer::RevertProperty(object, &component, property.name);
                            context.GetUndoService().RecordObjectChange(object.GetId(), before, Text(EditorText::UndoRevertProperty));
                        }
                        else
                        {
                            LevelSerializer::ApplyPropertyToAsset(object, &component, property.name);
                        }
                    };
                }
                if (isTransform)
                {
                    options.filter = [](const Reflection::PropertyInfo&) { return true; };
                    Level::Transform& transform = object.GetTransform();
                    for (const Reflection::PropertyInfo* property : transform.GetType().GetAllProperties())
                    {
                        Reflection::PropertyInfo display = *property;
                        if (display.name == "m_localPosition")
                        {
                            display.displayName = Text(EditorText::Position);
                        }
                        else if (display.name == "m_localRotation")
                        {
                            display.displayName = Text(EditorText::Rotation);
                        }
                        else if (display.name == "m_localScale")
                        {
                            display.displayName = Text(EditorText::Scale);
                        }
                        changed |= PropertyDrawer::DrawProperty(display, transform, options);
                    }
                }
                else
                {
                    changed |= PropertyDrawer::DrawObject(component, options);
                }
            }
            ImGui::Unindent(4.0f);
        }
        ImGui::PopID();
        if (removed)
        {
            object.RemoveComponent(&component);
            changed = true;
        }
        return changed;
    }

    void InspectorPanel::DrawAddComponent(LevelObject& object)
    {
        float width = std::min(260.0f, ImGui::GetContentRegionAvail().x);
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - width) * 0.5f + ImGui::GetCursorPosX());
        if (ImGui::Button(Text(EditorText::AddComponent), ImVec2(width, 0.0f)))
        {
            m_componentFilter.clear();
            ImGui::OpenPopup("AddComponentPopup");
        }
        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputTextWithHint("##search", Text(EditorText::SearchComponentsHint), &m_componentFilter);
            std::map<std::string, std::vector<const Reflection::TypeInfo*>> byCategory;
            for (const Reflection::TypeInfo* type : Reflection::TypeRegistry::GetDerived(Level::LevelObjectComponent::StaticType()))
            {
                if (type == &Level::Transform::StaticType() || type == &Level::MissingComponent::StaticType())
                {
                    continue;
                }
                std::string name = type->GetName();
                std::string lowerName = name;
                std::string lowerFilter = m_componentFilter;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                if (!lowerFilter.empty() && lowerName.find(lowerFilter) == std::string::npos)
                {
                    continue;
                }
                std::string category = type->GetCategory().empty() ? (type->GetModuleName() == Core::EngineModuleName ? Text(EditorText::EngineCategory) : Text(EditorText::GameCategory)) : type->GetCategory();
                byCategory[category].push_back(type);
            }
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
                        m_cachedObject = {};
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndPopup();
        }
    }

    void InspectorPanel::DrawObject(LevelObject& object)
    {
        EditorContext& context = m_application.GetContext();
        if (m_cachedObject != object.GetId() || (!ImGui::IsAnyItemActive() && !m_editInProgress && m_cachedRevision != context.GetUndoService().GetUndo().GetRevision() + context.GetLevelService().GetLevelRevision()))
        {
            m_before.SetNull();
            ObjectEditService::Snapshot(object, m_before);
            m_cachedObject = object.GetId();
            m_cachedRevision = context.GetUndoService().GetUndo().GetRevision() + context.GetLevelService().GetLevelRevision();
        }
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

        bool structural = true;
        if (!context.GetLevelObjectEditService().IsEditingLevelObject() && object.IsLinked())
        {
            LevelObject* linkRoot = object.GetLinkRoot();
            const Assets::AssetRecord* record = linkRoot ? Assets::AssetDatabase::Get().FindRecord(linkRoot->GetLink().asset) : nullptr;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColorVec4(EditorColor::Linked, 0.12f));
            ImGui::BeginChild("LinkBar", ImVec2(0, ImGui::GetFrameHeightWithSpacing() * 2.2f), ImGuiChildFlags_Borders);
            bool linkedToTemplate = record && record->type == Assets::AssetType::LevelObjectTemplate;
            ImGui::TextColored(EditorColorVec4(EditorColor::Linked), "%s", Text(linkedToTemplate ? EditorText::LinkedTemplate : EditorText::LinkedVariant, record ? record->name : std::string(Text(EditorText::MissingLink))).c_str());
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
                    return;
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
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    return;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(Text(linkedToTemplate ? EditorText::DetachFromTemplate : EditorText::Unlink)))
                {
                    Serialization::JsonDocument before;
                    ObjectEditService::Snapshot(*linkRoot, before);
                    LevelSerializer::Unlink(*linkRoot);
                    context.GetUndoService().RecordObjectChange(linkRoot->GetId(), before, Text(EditorText::Unlink));
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            structural = false;
        }
        if (context.GetLevelService().IsPlaying())
        {
            if (EditorPlaySession* session = m_application.GetPlaySession())
            {
                bool keeping = session->IsKeepingChanges(object.GetId());
                if (ImGui::Checkbox(Text(EditorText::KeepPlayModeChanges), &keeping))
                {
                    session->ToggleKeepChanges(object.GetId());
                }
            }
        }
        ImGui::Spacing();

        changed |= DrawComponent(object, object.GetTransform(), 0, 1, false);

        PropertyDrawOptions brainOptions;
        brainOptions.instanceContext = !context.GetLevelObjectEditService().IsEditingLevelObject();
        brainOptions.level = context.GetLevelService().GetActiveLevel();
        bool hasBrainProperties = false;
        for (const Reflection::PropertyInfo* property : object.GetType().GetAllProperties())
        {
            hasBrainProperties |= PropertyDrawer::IsVisible(*property, brainOptions);
        }
        if (hasBrainProperties)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader(object.GetType().GetName().c_str()))
            {
                if (object.IsLinked() && !context.GetLevelObjectEditService().IsEditingLevelObject())
                {
                    brainOptions.isOverridden = [&](const Reflection::PropertyInfo& property) { return LevelSerializer::IsPropertyOverridden(object, nullptr, property.name); };
                    brainOptions.onContextAction = [&](const Reflection::PropertyInfo& property, PropertyContextAction action)
                    {
                        if (action == PropertyContextAction::Revert)
                        {
                            Serialization::JsonDocument before;
                            ObjectEditService::Snapshot(object, before);
                            LevelSerializer::RevertProperty(object, nullptr, property.name);
                            context.GetUndoService().RecordObjectChange(object.GetId(), before, Text(EditorText::UndoRevertProperty));
                        }
                        else
                        {
                            LevelSerializer::ApplyPropertyToAsset(object, nullptr, property.name);
                        }
                    };
                }
                ImGui::Indent(4.0f);
                changed |= PropertyDrawer::DrawObject(object, brainOptions);
                ImGui::Unindent(4.0f);
            }
        }

        size_t count = object.GetComponents().size();
        for (size_t index = 0; index < object.GetComponents().size(); ++index)
        {
            LevelObjectComponent* component = object.GetComponents()[index].get();
            if (component->IsPendingDestroy())
            {
                continue;
            }
            bool componentChanged = DrawComponent(object, *component, index, count, structural || context.GetLevelService().IsPlaying());
            changed |= componentChanged;
            if (componentChanged && index < object.GetComponents().size() && object.GetComponents()[index].get() != component)
            {
                break;
            }
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (structural || context.GetLevelService().IsPlaying())
        {
            DrawAddComponent(object);
        }
        else
        {
            ImGui::TextDisabled("%s", Text(EditorText::UnlinkToAddComponents));
        }
        if (!context.GetLevelService().IsPlaying())
        {
            TrackEdit(object, changed);
        }
    }

    void InspectorPanel::DrawImportSettings(const Core::Guid& guid)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        const Assets::AssetRecord* record = database.FindRecord(guid);
        if (!record || database.IsBuiltin(guid))
        {
            return;
        }
        if (m_importSettingsAsset != guid)
        {
            m_importSettingsAsset = guid;
            m_importSettings = database.LoadImportSettings(*record);
            m_importSettingsDirty = false;
        }
        if (!m_importSettings)
        {
            return;
        }
        ImGui::SeparatorText(Text(EditorText::ImportSettings));
        PropertyDrawOptions options;
        options.instanceContext = false;
        if (PropertyDrawer::DrawObject(*m_importSettings, options))
        {
            m_importSettingsDirty = true;
        }
        ImGui::BeginDisabled(!m_importSettingsDirty);
        if (ImGui::Button(Text(EditorText::Apply)))
        {
            database.SaveImportSettings(guid, *m_importSettings);
            m_importSettingsDirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::Revert)))
        {
            m_importSettings = database.LoadImportSettings(*record);
            m_importSettingsDirty = false;
        }
        ImGui::EndDisabled();
    }

    void InspectorPanel::DrawAsset(const Core::Guid& guid)
    {
        EditorContext& context = m_application.GetContext();
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        const Assets::AssetRecord* record = database.FindRecord(guid);
        if (!record)
        {
            ImGui::TextDisabled("%s", Text(EditorText::AssetNotFound));
            return;
        }
        Assets::AssetRecord copy = *record;
        if (RHI::Texture* thumbnail = m_application.GetThumbnails().Get(guid))
        {
            ImGui::Image(ImGuiRenderer::ToTextureId(thumbnail), ImVec2(64, 64));
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        ImGui::TextUnformatted(copy.name.c_str());
        ImGui::TextDisabled("%s", Assets::AssetTypeName(copy.type));
        ImGui::TextDisabled("%s", copy.path.c_str());
        ImGui::EndGroup();
        ImGui::Separator();

        switch (copy.type)
        {
        case Assets::AssetType::Level:
            if (ImGui::Button(Text(EditorText::OpenLevel)))
            {
                context.GetLevelService().OpenLevel(guid);
            }
            ImGui::SameLine();
            if (ImGui::Button(Text(EditorText::SetStartupLevel)))
            {
                context.GetProjectService().GetProject().SetStartupLevel(guid);
                context.GetProjectService().GetProject().Save();
            }
            break;
        case Assets::AssetType::LevelObjectVariant:
            if (ImGui::Button(Text(EditorText::OpenLevelObject)))
            {
                context.GetLevelObjectEditService().OpenLevelObjectEditor(guid);
            }
            ImGui::SameLine();
            if (ImGui::Button(Text(EditorText::PlaceInLevel)))
            {
                context.GetObjectEditService().InstantiateLevelObject(guid, m_application.GetActiveLevelView().GetSpawnPoint());
            }
            break;
        case Assets::AssetType::LevelObjectTemplate:
            ImGui::TextWrapped("%s", Text(EditorText::TemplateExplanation));
            if (ImGui::Button(Text(EditorText::EditTemplate)))
            {
                context.GetLevelObjectEditService().OpenLevelObjectEditor(guid);
            }
            ImGui::SameLine();
            if (ImGui::Button(Text(EditorText::CreateVariant)))
            {
                Core::Guid created = context.GetAssetCreationService().CreateVariantFromTemplate(guid);
                if (created.IsValid())
                {
                    context.GetSelectionService().SelectAsset(created);
                }
            }
            break;
        case Assets::AssetType::Material:
        {
            auto material = database.Load<Rendering::MaterialAsset>(guid);
            if (!material)
            {
                break;
            }
            PropertyDrawOptions options;
            options.instanceContext = false;
            options.readOnly = database.IsBuiltin(guid);
            bool changed = PropertyDrawer::DrawObject(*material, options);
            if (changed)
            {
                m_materialEditing = true;
            }
            if (m_materialEditing && !ImGui::IsAnyItemActive())
            {
                m_materialEditing = false;
                Serialization::JsonDocument document;
                Serialization::SerializeProperties(*material, document, document.GetAllocator());
                database.SaveJsonAsset(guid, document);
            }
            break;
        }
        case Assets::AssetType::Texture:
        {
            auto texture = database.Load<Rendering::TextureAsset>(guid);
            if (texture)
            {
                ImGui::Text("%u x %u, %zu mips", texture->GetWidth(), texture->GetHeight(), texture->GetMips().size());
            }
            DrawImportSettings(guid);
            break;
        }
        case Assets::AssetType::Model:
        {
            auto model = database.Load<Geometry::ModelAsset>(guid);
            if (model)
            {
                size_t triangles = 0;
                for (const Geometry::MeshData& mesh : model->GetMeshes())
                {
                    triangles += mesh.indices.size() / 3;
                }
                ImGui::Text("%zu meshes, %zu nodes, %zu triangles", model->GetMeshes().size(), model->GetNodes().size(), triangles);
                if (ImGui::Button(Text(EditorText::PlaceInLevel)))
                {
                    context.GetObjectEditService().CreateModelObject(guid, copy.name.substr(0, copy.name.find('.')), m_application.GetActiveLevelView().GetSpawnPoint());
                }
            }
            DrawImportSettings(guid);
            break;
        }
        case Assets::AssetType::Screen:
        case Assets::AssetType::Widget:
        case Assets::AssetType::StyleSheet:
        {
            if (ImGui::Button(Text(EditorText::OpenInCodeEditor)))
            {
                m_application.OpenFileInEditor(database.GetAbsolutePath(copy).string(), 0);
            }
            auto text = database.Load<Assets::TextAsset>(guid);
            if (text)
            {
                ImGui::BeginChild("Preview", ImVec2(0, 0), ImGuiChildFlags_Borders);
                ImGui::TextUnformatted(text->GetText().c_str());
                ImGui::EndChild();
            }
            break;
        }
        default:
            break;
        }
    }

    void InspectorPanel::DrawCollection()
    {
        EditorContext& context = m_application.GetContext();
        Level::LevelCollection* collection = context.GetSelectionService().GetSelectedCollection();
        Level::Level* level = context.GetLevelService().GetActiveLevel();
        if (!collection || !level || context.GetLevelService().GetCollectionIndex(collection) < 0)
        {
            ImGui::TextDisabled("%s", Text(EditorText::NothingSelected));
            return;
        }
        ImGui::TextUnformatted(Text(EditorText::LevelCollection));
        std::string name = collection->GetName();
        PropertyDrawer::BeginRow(Text(EditorText::Name), false);
        if (ImGui::InputText("##name", &name, ImGuiInputTextFlags_EnterReturnsTrue) && !context.GetLevelService().IsPlaying())
        {
            collection->SetName(name);
            context.GetLevelService().MarkDirty(collection);
        }
        PropertyDrawer::EndRow();
        const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(collection->GetAsset());
        ImGui::TextDisabled("%s", Text(EditorText::AssetPath, record ? record->path : std::string(Text(EditorText::NotSavedYet))).c_str());
        ImGui::Text("%zu root objects", collection->GetRoots().size());
        if (!context.GetLevelService().IsPlaying() && !context.GetLevelObjectEditService().IsEditingLevelObject())
        {
            if (ImGui::Button(Text(EditorText::SetActive)))
            {
                context.GetLevelService().SetActiveCollection(collection);
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(level->GetCollections().size() <= 1);
            if (ImGui::Button(Text(EditorText::RemoveFromLevel)))
            {
                if (collection->IsDirty())
                {
                    Level::LevelSerializer::SaveCollection(*collection);
                }
                level->RemoveCollection(collection);
                context.GetSelectionService().SelectCollection(nullptr);
                context.GetLevelService().MarkDirty(level->GetDefaultCollection());
                context.GetUndoService().GetUndo().Clear();
            }
            ImGui::EndDisabled();
        }
    }

    void InspectorPanel::DrawLevel()
    {
        EditorContext& context = m_application.GetContext();
        Level::Level* level = context.GetLevelService().GetEditLevel();
        if (!level || context.GetLevelObjectEditService().IsEditingLevelObject())
        {
            return;
        }
        ImGui::TextUnformatted(Text(EditorText::LevelName, level->GetName()).c_str());
        for (Framework::GameplaySlot slot : { Framework::GameplaySlot::GameMode, Framework::GameplaySlot::LevelBehavior })
        {
            ImGui::PushID(static_cast<int>(slot));
            ImGui::SeparatorText(Framework::LevelGameplaySettings::GetBaseType(slot).GetName().c_str());
            if (DrawGameplaySettings(level->GetSettings(), slot, context.GetLevelService().IsPlaying()))
            {
                context.GetLevelService().MarkDirty(level->GetDefaultCollection());
            }
            ImGui::PopID();
        }

        ImGui::SeparatorText(Text(EditorText::LevelCollections));
        for (const auto& collection : level->GetCollections())
        {
            ImGui::BulletText("%s", collection->GetName().c_str());
        }
        if (!context.GetLevelService().IsPlaying())
        {
            if (ImGui::Button(Text(EditorText::NewCollection)))
            {
                Level::LevelCollection* created = level->AddCollection(Text(EditorText::DefaultCollectionName, level->GetCollections().size() + 1));
                Level::LevelSerializer::SaveCollection(*created);
                context.GetLevelService().MarkDirty(level->GetDefaultCollection());
                context.GetSelectionService().SelectCollection(created);
            }
            ImGui::SameLine();
            if (ImGui::BeginCombo("##addexisting", "Add Existing...", ImGuiComboFlags_NoArrowButton))
            {
                for (const Assets::AssetRecord& record : Assets::AssetDatabase::Get().GetRecords(Assets::AssetType::LevelCollection))
                {
                    if (level->FindCollection(record.guid))
                    {
                        continue;
                    }
                    if (ImGui::Selectable(record.path.c_str()))
                    {
                        Level::LevelSerializer::LoadCollectionInto(*level, record.guid);
                        context.GetLevelService().MarkDirty(level->GetDefaultCollection());
                        context.GetLevelService().BumpLevelRevision();
                    }
                }
                ImGui::EndCombo();
            }
        }
    }

    void InspectorPanel::Draw()
    {
        if (!ImGui::Begin(WindowTitle(EditorWindow::Inspector).c_str()))
        {
            ImGui::End();
            return;
        }
        EditorContext& context = m_application.GetContext();
        switch (context.GetSelectionService().GetFocus())
        {
        case SelectionFocus::Objects:
            if (LevelObject* object = context.GetSelectionService().GetPrimarySelection())
            {
                if (context.GetSelectionService().GetSelection().size() > 1)
                {
                    ImGui::TextDisabled("%zu objects selected (editing the last selected)", context.GetSelectionService().GetSelection().size());
                }
                DrawObject(*object);
            }
            break;
        case SelectionFocus::Asset:
            DrawAsset(context.GetSelectionService().GetSelectedAsset());
            break;
        case SelectionFocus::Collection:
            DrawCollection();
            break;
        case SelectionFocus::Level:
            DrawLevel();
            break;
        default:
            ImGui::TextDisabled("%s", Text(EditorText::SelectObjectOrAsset));
            break;
        }
        ImGui::End();
    }
}
