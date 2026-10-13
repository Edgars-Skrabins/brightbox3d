#include "Commands/KeyChord.h"

#include <iterator>

namespace BrightBox3D::Editor
{
    namespace
    {
        struct ModifierName
        {
            ImGuiKeyChord modifier;
            const char* symbol;
            const char* word;
        };

        const ModifierName Modifiers[] = {
            { ImGuiMod_Super, "\xe2\x8c\x83", "Super" },
            { ImGuiMod_Alt, "\xe2\x8c\xa5", "Alt" },
            { ImGuiMod_Shift, "\xe2\x87\xa7", "Shift" },
            { ImGuiMod_Ctrl, "\xe2\x8c\x98", "Ctrl" },
        };

        const ModifierName WordOrder[] = {
            Modifiers[3],
            Modifiers[2],
            Modifiers[1],
            Modifiers[0],
        };

        struct KeyName
        {
            ImGuiKey key;
            const char* symbol;
            const char* word;
        };

        const KeyName KeyNames[] = {
            { ImGuiKey_Comma, ",", "," },
            { ImGuiKey_Period, ".", "." },
            { ImGuiKey_Slash, "/", "/" },
            { ImGuiKey_Delete, "\xe2\x8c\xa6", "Del" },
            { ImGuiKey_Backspace, "\xe2\x8c\xab", "Backspace" },
            { ImGuiKey_Enter, "\xe2\x8f\x8e", "Enter" },
            { ImGuiKey_Escape, "\xe2\x8e\x8b", "Esc" },
        };

        std::string KeyText(ImGuiKey key, ShortcutStyle style)
        {
            for (const KeyName& name : KeyNames)
            {
                if (name.key == key)
                {
                    return style == ShortcutStyle::Symbols ? name.symbol : name.word;
                }
            }
            return ImGui::GetKeyName(key);
        }
    }

    ShortcutStyle PlatformShortcutStyle()
    {
#if defined(__APPLE__)
        return ShortcutStyle::Symbols;
#else
        return ShortcutStyle::Words;
#endif
    }

    std::string ShortcutText(const KeyChord& chord, ShortcutStyle style)
    {
        if (!chord.IsBound())
        {
            return {};
        }
        std::string text;
        if (style == ShortcutStyle::Symbols)
        {
            for (const ModifierName& name : Modifiers)
            {
                text += (chord.modifiers & name.modifier) ? name.symbol : "";
            }
            return text + KeyText(chord.key, style);
        }
        for (const ModifierName& name : WordOrder)
        {
            text += (chord.modifiers & name.modifier) ? std::string(name.word) + "+" : "";
        }
        return text + KeyText(chord.key, style);
    }

    std::string ShortcutText(const KeyChord& chord)
    {
        return ShortcutText(chord, PlatformShortcutStyle());
    }

    bool IsBindableKey(ImGuiKey key)
    {
        bool modifier = key >= ImGuiKey_LeftCtrl && key <= ImGuiKey_RightSuper;
        return key >= ImGuiKey_NamedKey_BEGIN && key < ImGuiKey_GamepadStart && !modifier;
    }

    std::optional<ImGuiKey> KeyFromName(std::string_view name)
    {
        if (name == ImGui::GetKeyName(ImGuiKey_None))
        {
            return ImGuiKey_None;
        }
        for (int index = ImGuiKey_NamedKey_BEGIN; index < ImGuiKey_GamepadStart; ++index)
        {
            ImGuiKey key = static_cast<ImGuiKey>(index);
            if (IsBindableKey(key) && name == ImGui::GetKeyName(key))
            {
                return key;
            }
        }
        return std::nullopt;
    }

    std::vector<std::string> ModifierNames(ImGuiKeyChord modifiers)
    {
        std::vector<std::string> names;
        for (const ModifierName& name : WordOrder)
        {
            if (modifiers & name.modifier)
            {
                names.push_back(name.word);
            }
        }
        return names;
    }

    std::optional<ImGuiKeyChord> ModifierFromName(std::string_view name)
    {
        for (const ModifierName& modifier : Modifiers)
        {
            if (name == modifier.word)
            {
                return modifier.modifier;
            }
        }
        return std::nullopt;
    }
}
