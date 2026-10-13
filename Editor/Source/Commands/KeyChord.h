#pragma once

#include <imgui.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace BrightBox3D::Editor
{
    enum class ShortcutStyle
    {
        Symbols,
        Words
    };

    struct KeyChord
    {
        ImGuiKey key = ImGuiKey_None;
        ImGuiKeyChord modifiers = 0;

        bool IsBound() const { return key != ImGuiKey_None; }
        ImGuiKeyChord ToImGui() const { return key | modifiers; }
        bool operator==(const KeyChord& other) const = default;
    };

    ShortcutStyle PlatformShortcutStyle();
    std::string ShortcutText(const KeyChord& chord, ShortcutStyle style);
    std::string ShortcutText(const KeyChord& chord);

    bool IsBindableKey(ImGuiKey key);
    std::optional<ImGuiKey> KeyFromName(std::string_view name);
    std::vector<std::string> ModifierNames(ImGuiKeyChord modifiers);
    std::optional<ImGuiKeyChord> ModifierFromName(std::string_view name);
}
