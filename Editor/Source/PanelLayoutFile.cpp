#include "PanelLayoutFile.h"

#include <algorithm>

namespace BrightBox3D::Editor::PanelLayoutFile
{
    namespace
    {
        constexpr const char* TypeKey = "type";
        constexpr const char* IndexKey = "index";
        constexpr const char* StateKey = "state";
        constexpr EditorWindow DockablePanels[] = { EditorWindow::LevelView, EditorWindow::LevelContents, EditorWindow::Inspector, EditorWindow::Assets, EditorWindow::Terminal };

        std::optional<PanelEntry> ReadEntry(const Serialization::JsonValue& value)
        {
            if (!value.IsObject())
            {
                return std::nullopt;
            }
            std::optional<EditorWindow> type = FindWindowById(Serialization::GetString(value, TypeKey));
            const Serialization::JsonValue* index = Serialization::FindMember(value, IndexKey);
            if (!type || !IsDockablePanel(*type) || !index || !index->IsInt() || index->GetInt() < 1)
            {
                return std::nullopt;
            }
            const Serialization::JsonValue* state = Serialization::FindMember(value, StateKey);
            return PanelEntry{ *type, index->GetInt(), state && state->IsObject() ? state : nullptr };
        }
    }

    bool IsDockablePanel(EditorWindow type)
    {
        return std::ranges::find(DockablePanels, type) != std::end(DockablePanels);
    }

    void Write(const std::vector<std::unique_ptr<EditorPanel>>& panels, Serialization::JsonDocument& document)
    {
        Serialization::JsonAllocator& allocator = document.GetAllocator();
        document.SetArray();
        for (const std::unique_ptr<EditorPanel>& panel : panels)
        {
            if (!panel->IsOpen())
            {
                continue;
            }
            Serialization::JsonValue entry(rapidjson::kObjectType);
            Serialization::SetMember(entry, TypeKey, Serialization::MakeString(GetWindowInfo(panel->GetWindowType()).id, allocator), allocator);
            Serialization::SetMember(entry, IndexKey, Serialization::JsonValue(panel->GetInstanceIndex()), allocator);
            Serialization::JsonValue state(rapidjson::kObjectType);
            panel->SaveState(state, allocator);
            Serialization::SetMember(entry, StateKey, std::move(state), allocator);
            document.PushBack(std::move(entry), allocator);
        }
    }

    std::optional<std::vector<PanelEntry>> Read(const Serialization::JsonValue& document)
    {
        if (!document.IsArray())
        {
            return std::nullopt;
        }
        std::vector<PanelEntry> entries;
        for (const Serialization::JsonValue& value : document.GetArray())
        {
            std::optional<PanelEntry> entry = ReadEntry(value);
            bool duplicate = entry && std::ranges::any_of(entries, [&](const PanelEntry& existing) { return existing.type == entry->type && existing.index == entry->index; });
            if (entry && !duplicate)
            {
                entries.push_back(*entry);
            }
        }
        return entries;
    }
}
