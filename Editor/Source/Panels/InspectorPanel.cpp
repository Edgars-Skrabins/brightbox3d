#include "Panels/InspectorPanel.h"

#include "Assets/AssetDatabase.h"
#include "Assets/TextAsset.h"
#include "Core/Log.h"
#include "Core/StringUtils.h"
#include "Panels/LevelViewPanel.h"
#include "Reflection/PropertyTraits.h"
#include "EditorApplication.h"
#include "EditorPlaySession.h"
#include "EditorPreferences.h"
#include "Framework/GameMode.h"
#include "Level/LevelSerializer.h"
#include "Level/MissingComponent.h"
#include "PropertyDrawer.h"
#include "Reflection/TypeRegistry.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/ModelAsset.h"
#include "Rendering/TextureAsset.h"
#include "Serialization/ObjectSerializer.h"
#include "Thumbnails.h"
#include "ImGui/ImGuiRenderer.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <map>

namespace BrightBox3D::Editor
{
    using Level::Component;
    using Level::LevelObjectInstance;
    using Level::LevelSerializer;

    InspectorPanel::InspectorPanel(EditorApplication& application) : m_application(application)
    {
    }

    InspectorPanel::~InspectorPanel() = default;

    void InspectorPanel::TrackEdit(LevelObjectInstance& object, bool changed)
    {
        EditorContext& context = m_application.GetContext();
        if (changed)
        {
            context.MarkDirty(object);
            if (ImGui::IsAnyItemActive())
            {
                m_editInProgress = true;
            }
            else
            {
                context.RecordObjectChange(object.GetId(), m_before, "Edit Property");
                m_editInProgress = false;
                m_cachedObject = {};
            }
        }
        else if (m_editInProgress && !ImGui::IsAnyItemActive())
        {
            context.RecordObjectChange(object.GetId(), m_before, "Edit Property");
            m_editInProgress = false;
            m_cachedObject = {};
        }
    }

    bool InspectorPanel::DrawGameModeSettings(Serialization::JsonValue& settings, Serialization::JsonAllocator& allocator, bool readOnly)
    {
        if (!settings.IsObject())
        {
            settings.SetObject();
        }
        std::string typeName = Serialization::GetString(settings, Serialization::TypeKey, "GameMode");
        bool changed = false;
        PropertyDrawer::BeginRow("GameMode Class", false);
        ImGui::BeginDisabled(readOnly);
        if (ImGui::BeginCombo("##gamemode", typeName.c_str()))
        {
            for (const Reflection::TypeInfo* type : Reflection::TypeRegistry::GetDerived(Framework::GameMode::StaticType()))
            {
                if (ImGui::Selectable(type->GetName().c_str(), type->GetName() == typeName))
                {
                    typeName = type->GetName();
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        PropertyDrawer::EndRow();
        const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(typeName);
        if (!type || !type->IsA(Framework::GameMode::StaticType()))
        {
            ImGui::TextColored(EditorColorVec4(EditorColor::Missing), "GameMode class '%s' is not loaded", typeName.c_str());
            return false;
        }
        std::unique_ptr<Reflection::Object> temporary(type->CreateInstance());
        Serialization::DeserializeProperties(*temporary, settings);
        PropertyDrawOptions options;
        options.instanceContext = false;
        options.readOnly = readOnly;
        changed |= PropertyDrawer::DrawObject(*temporary, options);
        if (changed)
        {
            Serialization::JsonValue updated;
            Serialization::SerializeObject(*temporary, updated, allocator);
            settings = std::move(updated);
        }
        return changed;
    }

    bool InspectorPanel::DrawComponent(LevelObjectInstance& object, Component& component, size_t index, size_t count, bool structural)
    {
        EditorContext& context = m_application.GetContext();
        bool changed = false;
        ImGui::PushID(component.GetLocalId().ToString().c_str());
        bool isTransform = &component == &object.GetTransform();
        auto* missing = Reflection::Cast<Level::MissingComponent>(&component);
        std::string title = missing ? missing->GetMissingTypeName() + " (missing)" : component.GetType().GetName();

        float checkboxSpace = isTransform ? 0.0f : ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        bool open = ImGui::CollapsingHeader(("##header" + title).c_str(), ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth);
        bool removed = false;
        if (!isTransform && ImGui::BeginPopupContextItem("ComponentContext"))
        {
            ImGui::BeginDisabled(!structural);
            if (ImGui::MenuItem("Remove Component"))
            {
                removed = true;
            }
            if (ImGui::MenuItem("Move Up", nullptr, false, index > 0))
            {
                object.MoveComponent(&component, static_cast<int>(index) - 1);
                changed = true;
            }
            if (ImGui::MenuItem("Move Down", nullptr, false, index + 1 < count))
            {
                object.MoveComponent(&component, static_cast<int>(index) + 1);
                changed = true;
            }
            ImGui::EndDisabled();
            if (ImGui::MenuItem("Reset"))
            {
                std::unique_ptr<Reflection::Object> fresh(component.GetType().CreateInstance());
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
        bool overriddenEnabled = !isTransform && LevelSerializer::IsPropertyOverridden(object, &component, "$enabled");
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
                ImGui::TextWrapped("The component class '%s' is not loaded. Its data is preserved and will be restored when the class exists again.", missing->GetMissingTypeName().c_str());
            }
            else
            {
                PropertyDrawOptions options;
                options.instanceContext = !context.IsEditingLevelObject();
                options.level = context.GetActiveLevel();
                if (object.IsLinked() && !context.IsEditingLevelObject())
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
                            EditorContext::Snapshot(object, before);
                            LevelSerializer::RevertProperty(object, &component, property.name);
                            context.RecordObjectChange(object.GetId(), before, "Revert Property");
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
                            display.displayName = "Position";
                        }
                        else if (display.name == "m_localRotation")
                        {
                            display.displayName = "Rotation";
                        }
                        else if (display.name == "m_localScale")
                        {
                            display.displayName = "Scale";
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

    void InspectorPanel::DrawAddComponent(LevelObjectInstance& object)
    {
        float width = std::min(260.0f, ImGui::GetContentRegionAvail().x);
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - width) * 0.5f + ImGui::GetCursorPosX());
        if (ImGui::Button("Add Component", ImVec2(width, 0.0f)))
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
            ImGui::InputTextWithHint("##search", "Search components", &m_componentFilter);
            std::map<std::string, std::vector<const Reflection::TypeInfo*>> byCategory;
            for (const Reflection::TypeInfo* type : Reflection::TypeRegistry::GetDerived(Level::Component::StaticType()))
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
                std::string category = type->GetCategory().empty() ? (type->GetModuleName() == "BrightBox3D" ? "Engine" : "Game") : type->GetCategory();
                byCategory[category].push_back(type);
            }
            for (auto& [category, types] : byCategory)
            {
                ImGui::SeparatorText(category.c_str());
                for (const Reflection::TypeInfo* type : types)
                {
                    if (ImGui::Selectable(type->GetName().c_str()))
                    {
                        Serialization::JsonDocument before;
                        EditorContext::Snapshot(object, before);
                        object.AddComponent(*type);
                        m_application.GetContext().RecordObjectChange(object.GetId(), before, "Add " + type->GetName());
                        m_cachedObject = {};
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndPopup();
        }
    }

    void InspectorPanel::DrawObject(LevelObjectInstance& object)
    {
        EditorContext& context = m_application.GetContext();
        if (m_cachedObject != object.GetId() || (!ImGui::IsAnyItemActive() && !m_editInProgress && m_cachedRevision != context.GetUndo().GetRevision() + context.GetLevelRevision()))
        {
            m_before.SetNull();
            EditorContext::Snapshot(object, m_before);
            m_cachedObject = object.GetId();
            m_cachedRevision = context.GetUndo().GetRevision() + context.GetLevelRevision();
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
        ImGui::TextDisabled("%s", object.GetMissingTypeName().empty() ? object.GetType().GetName().c_str() : (object.GetMissingTypeName() + " (missing type)").c_str());

        bool structural = true;
        if (!context.IsEditingLevelObject() && object.IsLinked())
        {
            LevelObjectInstance* linkRoot = object.GetLinkRoot();
            const Assets::AssetRecord* record = linkRoot ? Assets::AssetDatabase::Get().FindRecord(linkRoot->GetLink().asset) : nullptr;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColorVec4(EditorColor::Linked, 0.12f));
            ImGui::BeginChild("LinkBar", ImVec2(0, ImGui::GetFrameHeightWithSpacing() * 2.2f), ImGuiChildFlags_Borders);
            ImGui::TextColored(EditorColorVec4(EditorColor::Linked), "LevelObject: %s", record ? record->name.c_str() : "missing");
            if (linkRoot && !context.IsPlaying())
            {
                if (ImGui::SmallButton("Select"))
                {
                    context.SelectAsset(linkRoot->GetLink().asset);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Open"))
                {
                    Core::Guid asset = linkRoot->GetLink().asset;
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    context.OpenLevelObjectEditor(asset);
                    return;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Apply All"))
                {
                    LevelSerializer::ApplyAllToAsset(*linkRoot);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Revert All"))
                {
                    Serialization::JsonDocument before;
                    EditorContext::Snapshot(*linkRoot, before);
                    Core::Guid id = linkRoot->GetId();
                    LevelSerializer::RevertAll(*linkRoot);
                    context.RecordObjectChange(id, before, "Revert Overrides");
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    return;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Unlink"))
                {
                    Serialization::JsonDocument before;
                    EditorContext::Snapshot(*linkRoot, before);
                    LevelSerializer::Unlink(*linkRoot);
                    context.RecordObjectChange(linkRoot->GetId(), before, "Unlink");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            structural = false;
        }
        if (context.IsPlaying())
        {
            if (EditorPlaySession* session = m_application.GetPlaySession())
            {
                bool keeping = session->IsKeepingChanges(object.GetId());
                if (ImGui::Checkbox("Keep changes after PlayMode", &keeping))
                {
                    session->ToggleKeepChanges(object.GetId());
                }
            }
        }
        ImGui::Spacing();

        changed |= DrawComponent(object, object.GetTransform(), 0, 1, false);

        PropertyDrawOptions brainOptions;
        brainOptions.instanceContext = !context.IsEditingLevelObject();
        brainOptions.level = context.GetActiveLevel();
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
                if (object.IsLinked() && !context.IsEditingLevelObject())
                {
                    brainOptions.isOverridden = [&](const Reflection::PropertyInfo& property) { return LevelSerializer::IsPropertyOverridden(object, nullptr, property.name); };
                    brainOptions.onContextAction = [&](const Reflection::PropertyInfo& property, PropertyContextAction action)
                    {
                        if (action == PropertyContextAction::Revert)
                        {
                            Serialization::JsonDocument before;
                            EditorContext::Snapshot(object, before);
                            LevelSerializer::RevertProperty(object, nullptr, property.name);
                            context.RecordObjectChange(object.GetId(), before, "Revert Property");
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
            Component* component = object.GetComponents()[index].get();
            if (component->IsPendingDestroy())
            {
                continue;
            }
            bool componentChanged = DrawComponent(object, *component, index, count, structural || context.IsPlaying());
            changed |= componentChanged;
            if (componentChanged && index < object.GetComponents().size() && object.GetComponents()[index].get() != component)
            {
                break;
            }
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (structural || context.IsPlaying())
        {
            DrawAddComponent(object);
        }
        else
        {
            ImGui::TextDisabled("Unlink or open the LevelObject to add components");
        }
        if (!context.IsPlaying())
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
            database.LoadImportSettings(*record, m_importSettings);
            m_importSettingsDirty = false;
        }
        if (!m_importSettings.IsObject() || m_importSettings.MemberCount() == 0)
        {
            return;
        }
        ImGui::SeparatorText("Import Settings");
        for (auto it = m_importSettings.MemberBegin(); it != m_importSettings.MemberEnd(); ++it)
        {
            std::string name = it->name.GetString();
            std::string label = Core::PrettifyName(name);
            PropertyDrawer::BeginRow(label.c_str(), false);
            Serialization::JsonValue& value = it->value;
            if (value.IsBool())
            {
                bool flag = value.GetBool();
                if (ImGui::Checkbox("##value", &flag))
                {
                    value.SetBool(flag);
                    m_importSettingsDirty = true;
                }
            }
            else if (value.IsNumber())
            {
                float number = static_cast<float>(value.GetDouble());
                if (ImGui::DragFloat("##value", &number, 0.01f))
                {
                    value.SetDouble(number);
                    m_importSettingsDirty = true;
                }
            }
            else if (value.IsString())
            {
                std::string text = value.GetString();
                const Reflection::EnumInfo* enumInfo = name == "filter" ? &Reflection::StaticEnum<Rendering::TextureFilterMode>() : (name == "wrap" ? &Reflection::StaticEnum<Rendering::TextureWrapMode>() : nullptr);
                if (enumInfo)
                {
                    if (ImGui::BeginCombo("##value", text.c_str()))
                    {
                        for (const Reflection::EnumEntry& entry : enumInfo->entries)
                        {
                            if (ImGui::Selectable(entry.name.c_str(), entry.name == text))
                            {
                                value.SetString(entry.name.c_str(), m_importSettings.GetAllocator());
                                m_importSettingsDirty = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                else if (ImGui::InputText("##value", &text))
                {
                    value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), m_importSettings.GetAllocator());
                    m_importSettingsDirty = true;
                }
            }
            PropertyDrawer::EndRow();
        }
        ImGui::BeginDisabled(!m_importSettingsDirty);
        if (ImGui::Button("Apply"))
        {
            database.SaveImportSettings(guid, m_importSettings);
            m_importSettingsDirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert"))
        {
            database.LoadImportSettings(*record, m_importSettings);
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
            ImGui::TextDisabled("Asset not found");
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
            if (ImGui::Button("Open Level"))
            {
                context.OpenLevel(guid);
            }
            ImGui::SameLine();
            if (ImGui::Button("Set as Startup Level"))
            {
                context.GetProject().SetStartupLevel(guid);
                context.GetProject().Save();
            }
            break;
        case Assets::AssetType::LevelObject:
            if (ImGui::Button("Open LevelObject"))
            {
                context.OpenLevelObjectEditor(guid);
            }
            ImGui::SameLine();
            if (ImGui::Button("Place in Level"))
            {
                context.InstantiateLevelObject(guid, m_application.GetLevelView().GetSpawnPoint());
            }
            break;
        case Assets::AssetType::LevelObjectTemplate:
            ImGui::TextWrapped("Templates are only used to create LevelObjects. They cannot be placed or spawned.");
            if (ImGui::Button("Edit Template"))
            {
                context.OpenLevelObjectEditor(guid);
            }
            ImGui::SameLine();
            if (ImGui::Button("Create LevelObject"))
            {
                Core::Guid created = context.CreateLevelObjectFromTemplate(guid);
                if (created.IsValid())
                {
                    context.SelectAsset(created);
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
            auto model = database.Load<Rendering::ModelAsset>(guid);
            if (model)
            {
                size_t triangles = 0;
                for (const Rendering::MeshData& mesh : model->GetMeshes())
                {
                    triangles += mesh.indices.size() / 3;
                }
                ImGui::Text("%zu meshes, %zu nodes, %zu triangles", model->GetMeshes().size(), model->GetNodes().size(), triangles);
                if (ImGui::Button("Place in Level"))
                {
                    context.CreateModelObject(guid, copy.name.substr(0, copy.name.find('.')), m_application.GetLevelView().GetSpawnPoint());
                }
            }
            DrawImportSettings(guid);
            break;
        }
        case Assets::AssetType::Screen:
        case Assets::AssetType::Widget:
        case Assets::AssetType::StyleSheet:
        {
            if (ImGui::Button("Open in Code Editor"))
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
        Level::LevelCollection* collection = context.GetSelectedCollection();
        Level::Level* level = context.GetActiveLevel();
        if (!collection || !level || context.GetCollectionIndex(collection) < 0)
        {
            ImGui::TextDisabled("Nothing selected");
            return;
        }
        ImGui::Text("LevelCollection");
        std::string name = collection->GetName();
        PropertyDrawer::BeginRow("Name", false);
        if (ImGui::InputText("##name", &name, ImGuiInputTextFlags_EnterReturnsTrue) && !context.IsPlaying())
        {
            collection->SetName(name);
            context.MarkDirty(collection);
        }
        PropertyDrawer::EndRow();
        const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(collection->GetAsset());
        ImGui::TextDisabled("Asset: %s", record ? record->path.c_str() : "(not saved yet)");
        ImGui::Text("%zu root objects", collection->GetRoots().size());
        if (!context.IsPlaying() && !context.IsEditingLevelObject())
        {
            if (ImGui::Button("Set Active"))
            {
                context.SetActiveCollection(collection);
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(level->GetCollections().size() <= 1);
            if (ImGui::Button("Remove From Level"))
            {
                if (collection->IsDirty())
                {
                    Level::LevelSerializer::SaveCollection(*collection);
                }
                level->RemoveCollection(collection);
                context.SelectCollection(nullptr);
                context.MarkDirty(level->GetDefaultCollection());
                context.GetUndo().Clear();
            }
            ImGui::EndDisabled();
        }
    }

    void InspectorPanel::DrawLevel()
    {
        EditorContext& context = m_application.GetContext();
        Level::Level* level = context.GetEditLevel();
        if (!level || context.IsEditingLevelObject())
        {
            return;
        }
        ImGui::Text("Level: %s", level->GetName().c_str());
        ImGui::SeparatorText("GameMode");
        Serialization::JsonDocument& settings = level->GetSettings();
        bool overrideGameMode = Serialization::GetBool(settings, "overrideGameMode", false);
        if (ImGui::Checkbox("Override project GameMode", &overrideGameMode) && !context.IsPlaying())
        {
            Serialization::SetMember(settings, "overrideGameMode", Serialization::JsonValue(overrideGameMode), settings.GetAllocator());
            context.MarkDirty(level->GetDefaultCollection());
        }
        if (overrideGameMode)
        {
            Serialization::JsonValue* gameMode = const_cast<Serialization::JsonValue*>(Serialization::FindMember(settings, "gameMode"));
            if (!gameMode)
            {
                Serialization::JsonValue fresh(rapidjson::kObjectType);
                fresh.AddMember("$type", "GameMode", settings.GetAllocator());
                settings.AddMember("gameMode", fresh, settings.GetAllocator());
                gameMode = &settings["gameMode"];
            }
            if (DrawGameModeSettings(*gameMode, settings.GetAllocator(), context.IsPlaying()))
            {
                context.MarkDirty(level->GetDefaultCollection());
            }
        }
        else
        {
            ImGui::TextDisabled("Uses the project's default GameMode (Project Settings)");
        }

        ImGui::SeparatorText("LevelCollections");
        for (const auto& collection : level->GetCollections())
        {
            ImGui::BulletText("%s", collection->GetName().c_str());
        }
        if (!context.IsPlaying())
        {
            if (ImGui::Button("New Collection"))
            {
                Level::LevelCollection* created = level->AddCollection("Collection " + std::to_string(level->GetCollections().size() + 1));
                Level::LevelSerializer::SaveCollection(*created);
                context.MarkDirty(level->GetDefaultCollection());
                context.SelectCollection(created);
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
                        context.MarkDirty(level->GetDefaultCollection());
                        context.BumpLevelRevision();
                    }
                }
                ImGui::EndCombo();
            }
        }
    }

    void InspectorPanel::Draw()
    {
        if (!ImGui::Begin("Inspector"))
        {
            ImGui::End();
            return;
        }
        EditorContext& context = m_application.GetContext();
        switch (context.GetFocus())
        {
        case SelectionFocus::Objects:
            if (LevelObjectInstance* object = context.GetPrimarySelection())
            {
                if (context.GetSelection().size() > 1)
                {
                    ImGui::TextDisabled("%zu objects selected (editing the last selected)", context.GetSelection().size());
                }
                DrawObject(*object);
            }
            break;
        case SelectionFocus::Asset:
            DrawAsset(context.GetSelectedAsset());
            break;
        case SelectionFocus::Collection:
            DrawCollection();
            break;
        case SelectionFocus::Level:
            DrawLevel();
            break;
        default:
            ImGui::TextDisabled("Select an object or asset");
            break;
        }
        ImGui::End();
    }
}
