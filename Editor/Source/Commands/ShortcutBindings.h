#pragma once

#include "Serialization/Json.h"

namespace BrightBox3D::Editor
{
    class CommandRegistry;

    Serialization::JsonValue WriteShortcutBindings(const CommandRegistry& registry, Serialization::JsonAllocator& allocator);
    void ReadShortcutBindings(CommandRegistry& registry, const Serialization::JsonValue& bindings);
}
