#include "Commands/ShortcutBindings.h"

#include "Commands/CommandRegistry.h"
#include "Core/Log.h"

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr const char* KeyField = "key";
        constexpr const char* ModifiersField = "modifiers";

        std::optional<KeyChord> ReadChord(const std::string& command, const Serialization::JsonValue& value)
        {
            if (!value.IsObject())
            {
                Core::Log::Warning(Text(EditorText::ShortcutInvalid, command));
                return std::nullopt;
            }
            std::string keyName = Serialization::GetString(value, KeyField);
            std::optional<ImGuiKey> key = KeyFromName(keyName);
            if (!key)
            {
                Core::Log::Warning(Text(EditorText::ShortcutUnknownKey, command, keyName));
                return std::nullopt;
            }
            KeyChord chord{ *key, 0 };
            const Serialization::JsonValue* modifiers = Serialization::FindMember(value, ModifiersField);
            if (!modifiers)
            {
                return chord;
            }
            if (!modifiers->IsArray())
            {
                Core::Log::Warning(Text(EditorText::ShortcutInvalid, command));
                return std::nullopt;
            }
            for (const Serialization::JsonValue& modifier : modifiers->GetArray())
            {
                std::optional<ImGuiKeyChord> flag = modifier.IsString() ? ModifierFromName(modifier.GetString()) : std::nullopt;
                if (!flag)
                {
                    Core::Log::Warning(Text(EditorText::ShortcutInvalid, command));
                    return std::nullopt;
                }
                chord.modifiers |= *flag;
            }
            return chord;
        }
    }

    Serialization::JsonValue WriteShortcutBindings(const CommandRegistry& registry, Serialization::JsonAllocator& allocator)
    {
        Serialization::JsonValue bindings(rapidjson::kObjectType);
        for (size_t index = 0; index < static_cast<size_t>(CommandId::Count); ++index)
        {
            CommandId id = static_cast<CommandId>(index);
            const EditorCommand& command = registry.GetCommand(id);
            if (!command.execute || !registry.IsShortcutRebound(id))
            {
                continue;
            }
            const KeyChord& chord = registry.GetShortcut(id);
            Serialization::JsonValue value(rapidjson::kObjectType);
            Serialization::SetMember(value, KeyField, Serialization::MakeString(ImGui::GetKeyName(chord.key), allocator), allocator);
            Serialization::JsonValue modifiers(rapidjson::kArrayType);
            for (const std::string& name : ModifierNames(chord.modifiers))
            {
                modifiers.PushBack(Serialization::MakeString(name, allocator), allocator);
            }
            Serialization::SetMember(value, ModifiersField, std::move(modifiers), allocator);
            Serialization::SetMember(bindings, command.key, std::move(value), allocator);
        }
        return bindings;
    }

    void ReadShortcutBindings(CommandRegistry& registry, const Serialization::JsonValue& bindings)
    {
        if (!bindings.IsObject())
        {
            return;
        }
        for (auto member = bindings.MemberBegin(); member != bindings.MemberEnd(); ++member)
        {
            std::string commandKey = member->name.GetString();
            std::optional<CommandId> id = registry.FindCommand(commandKey);
            if (!id)
            {
                Core::Log::Warning(Text(EditorText::ShortcutUnknownCommand, commandKey));
                continue;
            }
            if (std::optional<KeyChord> chord = ReadChord(commandKey, member->value))
            {
                registry.SetShortcut(*id, *chord);
            }
        }
    }
}
