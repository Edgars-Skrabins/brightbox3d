#include "Panels/Inspector/LevelInspector.h"

#include "Assets/AssetDatabase.h"
#include "EditorContext.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "Level/LevelSerializer.h"
#include "PropertyDrawer.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace BrightBox3D::Editor::LevelInspector
{
    namespace
    {
        using Framework::LevelGameplaySettings;

        constexpr Framework::GameplaySlot GameplaySlots[] = { Framework::GameplaySlot::GameMode, Framework::GameplaySlot::LevelBehavior };

        bool DrawClassCombo(const Reflection::TypeInfo& base, Framework::GameplaySlot slot, std::string& className, bool readOnly)
        {
            bool changed = false;
            PropertyDrawer::BeginRow(Text(EditorText::ClassRowLabel, base.GetName()).c_str(), false);
            ImGui::BeginDisabled(readOnly);
            if (ImGui::BeginCombo("##class", className.empty() ? Text(EditorText::None) : className.c_str()))
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
            return changed;
        }

        void DrawCollectionList(EditorContext& context, Level::Level& level)
        {
            ImGui::SeparatorText(Text(EditorText::LevelCollections));
            for (const auto& collection : level.GetCollections())
            {
                ImGui::BulletText("%s", collection->GetName().c_str());
            }
            if (context.GetLevelService().IsPlaying())
            {
                return;
            }
            if (ImGui::Button(Text(EditorText::NewCollection)))
            {
                Level::LevelCollection* created = level.AddCollection(Text(EditorText::DefaultCollectionName, level.GetCollections().size() + 1));
                Level::LevelSerializer::SaveCollection(*created);
                context.GetLevelService().MarkDirty(level.GetDefaultCollection());
                context.GetSelectionService().SelectCollection(created);
            }
            ImGui::SameLine();
            if (ImGui::BeginCombo("##addexisting", Text(EditorText::AddExistingCollection), ImGuiComboFlags_NoArrowButton))
            {
                for (const Assets::AssetRecord& record : Assets::AssetDatabase::Get().GetRecords(Assets::AssetType::LevelCollection))
                {
                    if (level.FindCollection(record.guid))
                    {
                        continue;
                    }
                    if (ImGui::Selectable(record.path.c_str()))
                    {
                        Level::LevelSerializer::LoadCollectionInto(level, record.guid);
                        context.GetLevelService().MarkDirty(level.GetDefaultCollection());
                        context.GetLevelService().BumpLevelRevision();
                    }
                }
                ImGui::EndCombo();
            }
        }
    }

    bool DrawGameplaySettings(Serialization::JsonDocument& levelSettings, Framework::GameplaySlot slot, bool readOnly)
    {
        const Reflection::TypeInfo& base = LevelGameplaySettings::GetBaseType(slot);
        auto& allocator = levelSettings.GetAllocator();
        Serialization::JsonValue& entry = LevelGameplaySettings::GetOrCreateEntry(levelSettings, slot);
        std::string className = LevelGameplaySettings::GetClassName(entry);
        bool changed = DrawClassCombo(base, slot, className, readOnly);
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

    void DrawLevel(EditorContext& context)
    {
        Level::Level* level = context.GetLevelService().GetEditLevel();
        if (!level || context.GetLevelObjectEditService().IsEditingLevelObject())
        {
            return;
        }
        ImGui::TextUnformatted(Text(EditorText::LevelName, level->GetName()).c_str());
        for (Framework::GameplaySlot slot : GameplaySlots)
        {
            ImGui::PushID(static_cast<int>(slot));
            ImGui::SeparatorText(LevelGameplaySettings::GetBaseType(slot).GetName().c_str());
            if (DrawGameplaySettings(level->GetSettings(), slot, context.GetLevelService().IsPlaying()))
            {
                context.GetLevelService().MarkDirty(level->GetDefaultCollection());
            }
            ImGui::PopID();
        }
        DrawCollectionList(context, *level);
    }

    void DrawCollection(EditorContext& context, Level::LevelCollection* collection)
    {
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
        ImGui::TextUnformatted(Text(EditorText::CollectionRootCount, collection->GetRoots().size()).c_str());
        if (context.GetLevelService().IsPlaying() || context.GetLevelObjectEditService().IsEditingLevelObject())
        {
            return;
        }
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
